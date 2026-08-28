// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"

#include "base/check.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_prefs_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_client.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_injector.h"
#include "components/bookmarks/common/bookmark_pref_names.h"

BookmarkBarUIControllerImpl::BookmarkBarUIControllerImpl(
    std::unique_ptr<BookmarkBarUIControllerInjector> injector)
    : injector_(std::move(injector)) {}

BookmarkBarUIControllerImpl::~BookmarkBarUIControllerImpl() = default;

void BookmarkBarUIControllerImpl::Bind(BookmarkBarUIClient* client) {
  CHECK(!client_) << "BookmarkBarUIController is already bound to a client.";
  client_ = client;

  BookmarkBarPrefsAdapter* prefs_adapter = injector_->GetPrefsAdapter();
  prefs_adapter->AddObserver(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarUIControllerImpl::OnAppsPageShortcutVisibilityPrefChanged,
          base::Unretained(this)));
  prefs_adapter->AddObserver(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarUIControllerImpl::OnTabGroupsVisibilityPrefChanged,
          base::Unretained(this)));
  prefs_adapter->AddObserver(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarUIControllerImpl::OnShowManagedBookmarksPrefChanged,
          base::Unretained(this)));

  // Push initial states.
  OnAppsPageShortcutVisibilityPrefChanged();
  OnTabGroupsVisibilityPrefChanged();
  OnShowManagedBookmarksPrefChanged();
}

void BookmarkBarUIControllerImpl::OnAppsPageShortcutVisibilityPrefChanged() {
  if (client_) {
    client_->SetAppsPageShortcutVisibility(
        injector_->GetPrefsAdapter()->GetBoolean(
            bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  }
}

void BookmarkBarUIControllerImpl::OnTabGroupsVisibilityPrefChanged() {
  if (client_) {
    client_->SetSavedTabGroupsVisibility(
        injector_->GetPrefsAdapter()->GetBoolean(
            bookmarks::prefs::kShowTabGroupsInBookmarkBar));
  }
}

void BookmarkBarUIControllerImpl::OnShowManagedBookmarksPrefChanged() {
  if (client_) {
    client_->SetManagedBookmarksFolderVisibility(
        injector_->GetPrefsAdapter()->GetBoolean(
            bookmarks::prefs::kShowManagedBookmarksInBookmarkBar));
  }
}
