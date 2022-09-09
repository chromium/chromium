// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_TEST_BOOKMARK_NAVIGATION_WRAPPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_TEST_BOOKMARK_NAVIGATION_WRAPPER_H_

#include <vector>

#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

struct NavigateParams;

// Replacement BookmarkNavigationWrapper to support testing.
class TestingBookmarkNavigationWrapper
    : public chrome::BookmarkNavigationWrapper {
 public:
  TestingBookmarkNavigationWrapper();
  ~TestingBookmarkNavigationWrapper() override;

  TestingBookmarkNavigationWrapper(const TestingBookmarkNavigationWrapper&) =
      delete;
  TestingBookmarkNavigationWrapper& operator=(
      const TestingBookmarkNavigationWrapper&) = delete;

  base::WeakPtr<content::NavigationHandle> NavigateTo(
      NavigateParams* params) override;

  const std::vector<GURL>& urls() const { return urls_; }
  GURL last_url() const { return urls_.empty() ? GURL() : urls_.back(); }

  ui::PageTransition last_transition() const {
    return transitions_.empty() ? ui::PAGE_TRANSITION_LINK
                                : transitions_.back();
  }

 private:
  std::vector<GURL> urls_;
  std::vector<ui::PageTransition> transitions_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_TEST_BOOKMARK_NAVIGATION_WRAPPER_H_
