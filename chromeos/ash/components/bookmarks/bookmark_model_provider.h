// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOOKMARKS_BOOKMARK_MODEL_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOOKMARKS_BOOKMARK_MODEL_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace ash {

// Provides the bookmarks::BookmarkModel associated with a user to ChromeOS
// callers without forcing them to depend on //chrome/browser/bookmarks's
// Profile-keyed factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// bookmark_model_provider_impl.h).
class COMPONENT_EXPORT(BOOKMARK_MODEL_PROVIDER) BookmarkModelProvider {
 public:
  BookmarkModelProvider();
  BookmarkModelProvider(const BookmarkModelProvider&) = delete;
  BookmarkModelProvider& operator=(const BookmarkModelProvider&) = delete;
  virtual ~BookmarkModelProvider();

  // Returns the process-wide singleton.
  static BookmarkModelProvider& Get();

  // Returns the BookmarkModel associated with `account_id`, or nullptr if none
  // is available. The returned pointer is owned by the BrowserContext-keyed
  // service infrastructure; callers must not delete it.
  virtual bookmarks::BookmarkModel* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BOOKMARKS_BOOKMARK_MODEL_PROVIDER_H_
