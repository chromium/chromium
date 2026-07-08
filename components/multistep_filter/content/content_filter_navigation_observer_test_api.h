// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CONTENT_CONTENT_FILTER_NAVIGATION_OBSERVER_TEST_API_H_
#define COMPONENTS_MULTISTEP_FILTER_CONTENT_CONTENT_FILTER_NAVIGATION_OBSERVER_TEST_API_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/multistep_filter/content/content_filter_navigation_observer.h"
#include "components/multistep_filter/core/filter_tab_controller.h"

namespace multistep_filter {

class MultistepFilterUiDelegate;

class ContentFilterNavigationObserverTestApi {
 public:
  explicit ContentFilterNavigationObserverTestApi(
      ContentFilterNavigationObserver& observer)
      : observer_(observer) {}

  ContentFilterNavigationObserverTestApi(
      const ContentFilterNavigationObserverTestApi&) = delete;
  ContentFilterNavigationObserverTestApi& operator=(
      const ContentFilterNavigationObserverTestApi&) = delete;

  ~ContentFilterNavigationObserverTestApi() = default;

  MultistepFilterUiDelegate* GetDelegate() {
    return observer_->delegate_.get();
  }

  FilterTabController* GetTabController() {
    return observer_->tab_controller_.get();
  }

 private:
  const raw_ref<ContentFilterNavigationObserver> observer_;
};

inline ContentFilterNavigationObserverTestApi test_api(
    ContentFilterNavigationObserver& observer) {
  return ContentFilterNavigationObserverTestApi(observer);
}

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CONTENT_CONTENT_FILTER_NAVIGATION_OBSERVER_TEST_API_H_
