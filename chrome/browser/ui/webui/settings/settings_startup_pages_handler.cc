// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
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
  base::ListValue startup_pages;
  int page_count = startup_custom_pages_table_model_.RowCount();
  std::vector<GURL> urls = startup_custom_pages_table_model_.GetURLs();
  for (int i = 0; i < page_count; ++i) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("title", startup_custom_pages_table_model_.GetText(i, 0));
    entry->SetString("url", urls[i].spec());
    entry->SetString("tooltip",
                     startup_custom_pages_table_model_.GetTooltip(i));
    entry->SetInteger("modelIndex", i);
    startup_pages.Append(std::move(entry));
  }

  FireWebUIListener("update-startup-pages", startup_pages);
}

void StartupPagesHandler::OnItemsChanged(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsAdded(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsRemoved(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::HandleAddStartupPage(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());

  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::string url_string;
  CHECK(args->GetString(1, &url_string));

  GURL url;
  if (!settings_utils::FixupAndValidateStartupPage(url_string, &url)) {
    ResolveJavascriptCallback(*callback_id, base::Value(false));
    return;
  }

  int row_count = startup_custom_pages_table_model_.RowCount();
  int index;
  if (!args->GetInteger(1, &index) || index > row_count)
    index = row_count;

  startup_custom_pages_table_model_.Add(index, url);
  SaveStartupPagesPref();
  ResolveJavascriptCallback(*callback_id, base::Value(true));
}

void StartupPagesHandler::HandleEditStartupPage(const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 3U);
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  int index;
  CHECK(args->GetInteger(1, &index));

  if (index < 0 || index > startup_custom_pages_table_model_.RowCount()) {
    RejectJavascriptCallback(*callback_id, base::Value());
    NOTREACHED();
    return;
  }

  std::string url_string;
  CHECK(args->GetString(2, &url_string));

  GURL fixed_url;
  if (settings_utils::FixupAndValidateStartupPage(url_string, &fixed_url)) {
    std::vector<GURL> urls = startup_custom_pages_table_model_.GetURLs();
    urls[index] = fixed_url;
    startup_custom_pages_table_model_.SetURLs(urls);
    SaveStartupPagesPref();
    ResolveJavascriptCallback(*callback_id, base::Value(true));
  } else {
    ResolveJavascriptCallback(*callback_id, base::Value(false));
  }
}

void StartupPagesHandler::HandleOnStartupPrefsPageLoad(
    const base::ListValue* args) {
  AllowJavascript();
}

void StartupPagesHandler::HandleRemoveStartupPage(const base::ListValue* args) {
  int selected_index;
  if (!args->GetInteger(0, &selected_index)) {
    NOTREACHED();
    return;
  }

  if (selected_index < 0 ||
      selected_index >= startup_custom_pages_table_model_.RowCount()) {
    NOTREACHED();
    return;
  }

  startup_custom_pages_table_model_.Remove(selected_index);
  SaveStartupPagesPref();
}

void StartupPagesHandler::HandleSetStartupPagesToCurrentPages(
    const base::ListValue* args) {
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
