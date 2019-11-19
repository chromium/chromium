// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_FIND_RESULT_WAITER_H_
#define CHROME_TEST_BASE_FIND_RESULT_WAITER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/find_bar/find_result_observer.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
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
//
// Always construct FindResultWaiter AFTER initiating the search. It captures
// the current search ID in the constructor and waits for it only.
class FindResultWaiter : public FindResultObserver {
 public:
  explicit FindResultWaiter(content::WebContents* parent_tab);
  ~FindResultWaiter() override;

  void Wait();

  int active_match_ordinal() const { return active_match_ordinal_; }
  int number_of_matches() const { return number_of_matches_; }
  gfx::Rect selection_rect() const { return selection_rect_; }

 private:
  // FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;

  std::unique_ptr<base::RunLoop> run_loop_;
  ScopedObserver<FindTabHelper, FindResultObserver> observer_{this};

  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_ = -1;
  int number_of_matches_ = 0;
  gfx::Rect selection_rect_;
  // The id of the current find request, obtained from WebContents. Allows us
  // to monitor when the search completes.
  int current_find_request_id_ = 0;

  bool seen_ = false;  // true after transition to expected state has been seen

  DISALLOW_COPY_AND_ASSIGN(FindResultWaiter);
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_FIND_RESULT_WAITER_H_
