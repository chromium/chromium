// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_CORE_BOOKMARK_MODEL_H_
#define COMPONENTS_BOOKMARKS_BROWSER_CORE_BOOKMARK_MODEL_H_

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace bookmarks {

// A minimal subset of BookmarkModel API, intended to allow migrating the ios/
// codebase from having two BookmarkModel instances to having one. One
// important property of all APIs in this class is that they have obvious
// semantics if an instance represented a merged view of two underlying
// BookmarkModel instances. Beyond that, the precise APIs included here is
// arbitrary and influenced by actual need in code.
// TODO(crbug.com/326185948): Remove this base class one the migration is
// complete.
class CoreBookmarkModel : public KeyedService {
 public:
  CoreBookmarkModel();
  CoreBookmarkModel(const CoreBookmarkModel&) = delete;
  ~CoreBookmarkModel() override;

  CoreBookmarkModel& operator=(const CoreBookmarkModel&) = delete;

  // Returns true if the specified URL is bookmarked.
  virtual bool IsBookmarked(const GURL& url) const = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_CORE_BOOKMARK_MODEL_H_
