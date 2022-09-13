// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_END_REASON_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_END_REASON_H_

namespace page_load_metrics {

// This enum represents how a page load ends. If the action occurs before the
// page load finishes (or reaches some point like first paint), then we consider
// the load to be aborted.
//
// These values are persisted to logs and the history DB. Entries should not be
// renumbered and numeric values should never be reused. For any additions, also
// update the corresponding PageEndReason enum in enums.xml.
enum PageEndReason {
  // Page lifetime has not yet ended (page is still active). Pages that
  // become hidden are logged in END_HIDDEN, so we expect low numbers in this
  // bucket from the end of May 2020.
  END_NONE = 0,

  // The page was reloaded, possibly by the user.
  END_RELOAD = 1,

  // The page was navigated away from, via a back or forward navigation.
  END_FORWARD_BACK = 2,

  // The navigation is replaced with a navigation with the qualifier
  // ui::PAGE_TRANSITION_CLIENT_REDIRECT, which is caused by Javascript, or the
  // meta refresh tag.
  END_CLIENT_REDIRECT = 3,

  // If the page load is replaced by a new navigation. This includes link
  // clicks, typing in the omnibox (not a reload), and form submissions.
  END_NEW_NAVIGATION = 4,

  // The page load was stopped (e.g. the user presses the stop X button).
  END_STOP = 5,

  // Page load ended due to closing the tab or browser.
  END_CLOSE = 6,

  // The provisional load for this page load failed before committing.
  END_PROVISIONAL_LOAD_FAILED = 7,

  // The render process hosting the page terminated unexpectedly.
  END_RENDER_PROCESS_GONE = 8,

  // We don't know why the page load ended. This is the value we assign to a
  // terminated provisional load if the only signal we get is the load finished
  // without committing, either without error or with net::ERR_ABORTED.
  END_OTHER = 9,

  // The page became hidden but is still active. Added end of May 2020 for
  // Navigation.PageEndReason2. Deprecated Jan 2021 with
  // Navigation.PageEndReason3.
  END_HIDDEN = 10,

  // The page load has not yet ended but we need to flush the metrics because
  // the app has entered the background.
  END_APP_ENTER_BACKGROUND = 11,

  PAGE_END_REASON_COUNT
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_END_REASON_H_
