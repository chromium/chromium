// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"

BookmarksMessageHandler::BookmarksMessageHandler() = default;

BookmarksMessageHandler::~BookmarksMessageHandler() = default;

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
  web_ui()->RegisterMessageCallback(
      "getCanUploadBookmarkToAccountStorage",
      base::BindRepeating(
          &BookmarksMessageHandler::HandleGetCanUploadBookmarkToAccountStorage,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onSingleBookmarkUploadClicked",
      base::BindRepeating(&BookmarksMessageHandler::HandleSingleUploadClicked,
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

bool BookmarksMessageHandler::CanUploadBookmarkToAccountStorage(
    const std::string& id_string) {
  int64_t id;

  // Check if the bookmark's id is valid.
  if (!base::StringToInt64(id_string, &id)) {
    return false;
  }

  // Do not proceed if bookmarks cannot be edited.
  if (!CanEditBookmarks()) {
    return false;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);

  // Do not proceed if the bookmark does not exist.
  if (!node) {
    return false;
  }

  // Do not proceed if the node is a permanent node.
  if (model->is_permanent_node(node)) {
    return false;
  }

  // Do not proceed if the user is not using account storage.
  if (!model->account_other_node()) {
    return false;
  }

  // Do not proceed if the bookmark is managed.
  if (ManagedBookmarkServiceFactory::GetForProfile(profile)->IsNodeManaged(
          node)) {
    return false;
  }

  // Do not proceed if the bookmark is already in the account storage, or if the
  // user is syncing.
  if (!model->IsLocalOnlyNode(*node)) {
    return false;
  }

  return true;
}

void BookmarksMessageHandler::HandleGetCanUploadBookmarkToAccountStorage(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  std::string id = args[1].GetString();

  AllowJavascript();

  ResolveJavascriptCallback(callback_id,
                            base::Value(CanUploadBookmarkToAccountStorage(id)));
}

void BookmarksMessageHandler::HandleSingleUploadClicked(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  std::string id_string = args[0].GetString();
  int64_t id;
  base::StringToInt64(id_string, &id);

  Profile* profile = Profile::FromWebUI(web_ui());
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  // Do not continue if account nodes are no longer available. This can happen
  // if the user signs out and the UI is not updated properly.
  // TODO(crbug.com/413637312): Remove this once the icon is no longer visible
  // upon sign out.
  if (!model->account_other_node()) {
    return;
  }

  // All conditions for uploading to account storage should be met at this
  // point.
  CHECK(CanUploadBookmarkToAccountStorage(id_string));

  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);

  // If the dialog is accepted, move it to the permanent account node
  // corresponding to the permanent local node it is saved under.
  const bookmarks::BookmarkPermanentNode* parent_node = nullptr;
  if (node->HasAncestor(model->other_node())) {
    parent_node = model->account_other_node();
  } else if (node->HasAncestor(model->bookmark_bar_node())) {
    parent_node = model->account_bookmark_bar_node();
  } else if (node->HasAncestor(model->mobile_node())) {
    parent_node = model->account_mobile_node();
  }
  CHECK(parent_node);

  // Show the dialog asking the user to confirm their choice to move the
  // bookmark.
  ShowBookmarkAccountStorageMoveDialog(
      chrome::FindLastActiveWithProfile(profile), node, parent_node,
      parent_node->children().size(),
      BookmarkAccountStorageMoveDialogType::kUpload);
}

void BookmarksMessageHandler::UpdateCanEditBookmarks() {
  FireWebUIListener("can-edit-bookmarks-changed",
                    base::Value(CanEditBookmarks()));
}
