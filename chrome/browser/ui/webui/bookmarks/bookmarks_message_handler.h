// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_MESSAGE_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_promo_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
  void HandleGetIncognitoAvailability(const base::ListValue& args);
  void UpdateIncognitoAvailability();

  bool CanEditBookmarks();
  void HandleGetCanEditBookmarks(const base::ListValue& args);
  void UpdateCanEditBookmarks();

  bool CanUploadBookmarkToAccountStorage(const std::string& id);
  void HandleGetCanUploadBookmarkToAccountStorage(const base::ListValue& args);
  void HandleSingleUploadClicked(const base::ListValue& args);
  void HandleOpenBookmarks(const base::ListValue& args);

  void EnsurePromoDelegatesInitialized();
  void UpdateActivePromoDelegate();

  void HandleGetPromoData(const base::ListValue& args);
  void HandleOnPromoShown(const base::ListValue& args);
  void HandleOnPromoClicked(const base::ListValue& args);
  void HandleOnPromoDismissed(const base::ListValue& args);

  void OnPromoDataReceived(base::Value callback_id,
                           const BookmarkPromoDelegate* delegate,
                           BookmarkPromoData promo_data);
  void FirePromoDataUpdated(const BookmarkPromoDelegate* delegate,
                            BookmarkPromoData promo_data);
  void RequestPromoDataUpdate();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

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

  // These values are needed to only request a promo data update once after a
  // batch update that may change bookmarks' storage from local to account.
  bool batch_updates_ongoing_ = false;
  bool need_promo_data_update_ = false;

  // Keep track of the previous bookmarks sync state to filter out irrelevant
  // updates coming from `SyncService`.
  bool is_bookmarks_sync_active_ = false;

  std::vector<std::unique_ptr<BookmarkPromoDelegate>> promo_delegates_;
  raw_ptr<BookmarkPromoDelegate> active_promo_delegate_ = nullptr;

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
