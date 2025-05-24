// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class BookmarksMessageHandler : public content::WebUIMessageHandler,
                                public signin::IdentityManager::Observer,
                                public syncer::SyncServiceObserver,
                                public bookmarks::BookmarkModelObserver {
 public:
  BookmarksMessageHandler();

  BookmarksMessageHandler(const BookmarksMessageHandler&) = delete;
  BookmarksMessageHandler& operator=(const BookmarksMessageHandler&) = delete;

  ~BookmarksMessageHandler() override;

 private:
  friend class BookmarkMessageHandlerTest;

  int GetIncognitoAvailability();
  void HandleGetIncognitoAvailability(const base::Value::List& args);
  void UpdateIncognitoAvailability();

  bool CanEditBookmarks();
  void HandleGetCanEditBookmarks(const base::Value::List& args);
  void UpdateCanEditBookmarks();

  bool CanUploadBookmarkToAccountStorage(const std::string& id);
  void HandleGetCanUploadBookmarkToAccountStorage(
      const base::Value::List& args);
  void HandleSingleUploadClicked(const base::Value::List& args);

  void HandleGetBatchUploadPromoData(const base::Value::List& args);
  void HandleOnBatchUploadPromoClicked(const base::Value::List& args);
  void HandleOnBatchUploadPromoDismissed(const base::Value::List& args);

  void OnGetLocalDataDescriptionReceived(
      base::Value callback_id,
      std::map<syncer::DataType, syncer::LocalDataDescription> data);
  void FireOnGetLocalDataDescriptionReceived(
      std::map<syncer::DataType, syncer::LocalDataDescription> data);
  void RequestLocalDataDescriptionsUpdate();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;

  // bookmarks::BookmarkModelObserver
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) override;

  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override {}

  void RequestUpdateOrWaitForBatchUpdateEnd();

  // These values are needed to only request a local data update count once
  // after a batch update that may change bookmarks' storage from local to
  // account.
  bool batch_updates_ongoing_ = false;
  bool need_local_count_update_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};

  base::WeakPtrFactory<BookmarksMessageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
