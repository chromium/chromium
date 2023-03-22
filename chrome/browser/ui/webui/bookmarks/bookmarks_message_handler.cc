// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

BookmarksMessageHandler::BookmarksMessageHandler() {}

BookmarksMessageHandler::~BookmarksMessageHandler() {}

void BookmarksMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getIncognitoAvailability",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetIncognitoAvailability,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCanEditBookmarks",
      base::BindRepeating(&BookmarksMessageHandler::HandleGetCanEditBookmarks,
                          base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptAllowed() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(&BookmarksMessageHandler::UpdateIncognitoAvailability,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::BindRepeating(&BookmarksMessageHandler::UpdateCanEditBookmarks,
                          base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
}

int BookmarksMessageHandler::GetIncognitoAvailability() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetInteger(policy::policy_prefs::kIncognitoModeAvailability);
}

void BookmarksMessageHandler::HandleGetIncognitoAvailability(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(GetIncognitoAvailability()));
}

void BookmarksMessageHandler::UpdateIncognitoAvailability() {
  FireWebUIListener("incognito-availability-changed",
                    base::Value(GetIncognitoAvailability()));
}

bool BookmarksMessageHandler::CanEditBookmarks() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

void BookmarksMessageHandler::HandleGetCanEditBookmarks(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  AllowJavascript();

  ResolveJavascriptCallback(callback_id, base::Value(CanEditBookmarks()));
}

void BookmarksMessageHandler::UpdateCanEditBookmarks() {
  FireWebUIListener("can-edit-bookmarks-changed",
                    base::Value(CanEditBookmarks()));
}
