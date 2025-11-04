// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

// This class has been temporarily factored out of
// TabSearchBubbleHost, in order to facilitate the componentization of
// c/b/ui/toolbar/pinned_toolbar/.
//
// This happens because the header where it was original declared as an inner
// class (c/b/ui/views/tab_search_bubble_host.h) is included by
// c/b/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h,
// and tab_search_bubble_host.h itself includes various headers that are
// still built as part of //c/b/ui:ui GN target, ie they are not componentized
// yet.
//
// Hence, componentizing tab_search_toolbar_button_controller.h would create a
// circular dependency against //c/b/ui:ui and //c/b/ui/toolbar/pinned_toolbar
// from one of its headers, which is discouraged.
//
// TODO(crbug.com/369436587): Declare this class back as an inner class when:
// - tab_organization_observer.h
// - tab_slot_controller.h
// - tab_search_ui.h
// .. are componentized.
class TabSearchBubbleHostObserver : public base::CheckedObserver {
 public:
  virtual void OnBubbleInitializing() {}
  virtual void OnBubbleDestroying() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_BUBBLE_HOST_OBSERVER_H_
