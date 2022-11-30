// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/test_bookmark_navigation_wrapper.h"

#include "chrome/browser/ui/browser_navigator_params.h"

TestingBookmarkNavigationWrapper::TestingBookmarkNavigationWrapper() = default;
TestingBookmarkNavigationWrapper::~TestingBookmarkNavigationWrapper() = default;

base::WeakPtr<content::NavigationHandle>
TestingBookmarkNavigationWrapper::NavigateTo(NavigateParams* params) {
  urls_.push_back(params->url);
  transitions_.push_back(params->transition);
  return nullptr;
}
