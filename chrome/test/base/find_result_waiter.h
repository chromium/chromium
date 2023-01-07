// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_FIND_RESULT_WAITER_H_
#define CHROME_TEST_BASE_FIND_RESULT_WAITER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/find_in_page/find_tab_helper.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class RunLoop;
}

namespace content {
class WebContents;
}  // namespace content

namespace ui_test_utils {

// FindResultWaiter allows blocking UI thread until find results are available.
// Typical usage:
//
//   FindInPageWchar();
//   FindResultWaiter observer(tab);
//   observer.Wait();
class FindResultWaiter : public find_in_page::FindResultObserver {
 public:
  // |request_offset| will be added to the current find request ID; the
  // resulting ID will be the only one waited on. Typically, FindResultWaiter
  // is constructed AFTER initiating a search, with request_offset set to 0. In
  // such cases you must be sure the find-result callback won't already have
  // been called. Otherwise, you can construct FindResultWaiter BEFORE starting
  // a search and set request_offset to 1 (or whatever offset is appropriate).
  explicit FindResultWaiter(content::WebContents* parent_tab,
                            int request_offset = 0);
  FindResultWaiter(const FindResultWaiter&) = delete;
  FindResultWaiter& operator=(const FindResultWaiter&) = delete;
  ~FindResultWaiter() override;

  void Wait();

  int active_match_ordinal() const { return active_match_ordinal_; }
  int number_of_matches() const { return number_of_matches_; }
  gfx::Rect selection_rect() const { return selection_rect_; }

 private:
  // find_in_page::FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;

  std::unique_ptr<base::RunLoop> run_loop_;
  base::ScopedObservation<find_in_page::FindTabHelper,
                          find_in_page::FindResultObserver>
      observation_{this};

  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_ = -1;
  int number_of_matches_ = 0;
  gfx::Rect selection_rect_;
  // The id of the current find request, obtained from WebContents. Allows us
  // to monitor when the search completes.
  int current_find_request_id_ = 0;

  bool seen_ = false;  // true after transition to expected state has been seen
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_FIND_RESULT_WAITER_H_
