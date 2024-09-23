// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace settings {

StartupPagesHandler::StartupPagesHandler(content::WebUI* webui)
    : startup_custom_pages_table_model_(Profile::FromWebUI(webui)) {
}

StartupPagesHandler::~StartupPagesHandler() {
}

void StartupPagesHandler::RegisterMessages() {
  if (Profile::FromWebUI(web_ui())->IsOffTheRecord())
    return;

  web_ui()->RegisterMessageCallback(
      "addStartupPage",
      base::BindRepeating(&StartupPagesHandler::HandleAddStartupPage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "editStartupPage",
      base::BindRepeating(&StartupPagesHandler::HandleEditStartupPage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onStartupPrefsPageLoad",
      base::BindRepeating(&StartupPagesHandler::HandleOnStartupPrefsPageLoad,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeStartupPage",
      base::BindRepeating(&StartupPagesHandler::HandleRemoveStartupPage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setStartupPagesToCurrentPages",
      base::BindRepeating(
          &StartupPagesHandler::HandleSetStartupPagesToCurrentPages,
          base::Unretained(this)));
}

void StartupPagesHandler::OnJavascriptAllowed() {
  startup_custom_pages_table_model_.SetObserver(this);

  PrefService* prefService = Profile::FromWebUI(web_ui())->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefService);
  startup_custom_pages_table_model_.SetURLs(pref.urls);

  if (pref.urls.empty())
    pref.type = SessionStartupPref::DEFAULT;

  pref_change_registrar_.Init(prefService);
  pref_change_registrar_.Add(
      prefs::kURLsToRestoreOnStartup,
      base::BindRepeating(&StartupPagesHandler::UpdateStartupPages,
                          base::Unretained(this)));
}

void StartupPagesHandler::OnJavascriptDisallowed() {
  startup_custom_pages_table_model_.SetObserver(nullptr);
  pref_change_registrar_.RemoveAll();
}

void StartupPagesHandler::OnModelChanged() {
  base::Value::List startup_pages;
  size_t page_count = startup_custom_pages_table_model_.RowCount();
  std::vector<GURL> urls = startup_custom_pages_table_model_.GetURLs();
  for (size_t i = 0; i < page_count; ++i) {
    base::Value::Dict entry;
    entry.Set("title", startup_custom_pages_table_model_.GetText(i, 0));
    std::string spec;
    if (urls[i].is_valid()) {
      spec = urls[i].spec();
    }
    entry.Set("url", std::move(spec));
    entry.Set("tooltip", startup_custom_pages_table_model_.GetTooltip(i));
    entry.Set("modelIndex", base::checked_cast<int>(i));
    startup_pages.Append(std::move(entry));
  }

  FireWebUIListener("update-startup-pages",
                    base::Value(std::move(startup_pages)));
}

void StartupPagesHandler::OnItemsChanged(size_t start, size_t length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsAdded(size_t start, size_t length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsRemoved(size_t start, size_t length) {
  OnModelChanged();
}

void StartupPagesHandler::HandleAddStartupPage(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];

  if (!args[1].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::string url_string = args[1].GetString();

  GURL url;
  if (!settings_utils::FixupAndValidateStartupPage(url_string, &url)) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }

  startup_custom_pages_table_model_.Add(
      startup_custom_pages_table_model_.RowCount(), url);
  SaveStartupPagesPref();
  ResolveJavascriptCallback(callback_id, base::Value(true));
}

void StartupPagesHandler::HandleEditStartupPage(const base::Value::List& args) {
  CHECK_EQ(args.size(), 3U);
  const base::Value& callback_id = args[0];
  int index = args[1].GetInt();

  if (index < 0 || static_cast<size_t>(index) >=
                       startup_custom_pages_table_model_.RowCount()) {
    RejectJavascriptCallback(callback_id, base::Value());
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::string url_string = args[2].GetString();

  GURL fixed_url;
  if (settings_utils::FixupAndValidateStartupPage(url_string, &fixed_url)) {
    std::vector<GURL> urls = startup_custom_pages_table_model_.GetURLs();
    urls[index] = fixed_url;
    startup_custom_pages_table_model_.SetURLs(urls);
    SaveStartupPagesPref();
    ResolveJavascriptCallback(callback_id, base::Value(true));
  } else {
    ResolveJavascriptCallback(callback_id, base::Value(false));
  }
}

void StartupPagesHandler::HandleOnStartupPrefsPageLoad(
    const base::Value::List& args) {
  AllowJavascript();
}

void StartupPagesHandler::HandleRemoveStartupPage(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1u);
  if (!args[0].is_int()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  int selected_index = args[0].GetInt();

  if (selected_index < 0 || static_cast<size_t>(selected_index) >=
                                startup_custom_pages_table_model_.RowCount()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  startup_custom_pages_table_model_.Remove(selected_index);
  SaveStartupPagesPref();
}

void StartupPagesHandler::HandleSetStartupPagesToCurrentPages(
    const base::Value::List& args) {
  startup_custom_pages_table_model_.SetToCurrentlyOpenPages(
      web_ui()->GetWebContents());
  SaveStartupPagesPref();
}

void StartupPagesHandler::SaveStartupPagesPref() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);
  pref.urls = startup_custom_pages_table_model_.GetURLs();

  if (pref.urls.empty())
    pref.type = SessionStartupPref::DEFAULT;

  SessionStartupPref::SetStartupPref(prefs, pref);
}

void StartupPagesHandler::UpdateStartupPages() {
  const SessionStartupPref startup_pref = SessionStartupPref::GetStartupPref(
      Profile::FromWebUI(web_ui())->GetPrefs());
  startup_custom_pages_table_model_.SetURLs(startup_pref.urls);
  // The change will go to the JS code in the
  // StartupPagesHandler::OnModelChanged() method.
}

}  // namespace settings
