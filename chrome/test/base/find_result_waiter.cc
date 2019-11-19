// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/find_result_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "content/public/test/test_utils.h"

namespace ui_test_utils {

FindResultWaiter::FindResultWaiter(content::WebContents* parent_tab) {
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(parent_tab);
  current_find_request_id_ = find_tab_helper->current_find_request_id();
  observer_.Add(find_tab_helper);
}

FindResultWaiter::~FindResultWaiter() = default;

void FindResultWaiter::Wait() {
  if (seen_)
    return;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
}

void FindResultWaiter::OnFindResultAvailable(
    content::WebContents* web_contents) {
  const FindNotificationDetails& find_details =
      FindTabHelper::FromWebContents(web_contents)->find_result();

  if (find_details.request_id() != current_find_request_id_)
    return;

  // We get multiple responses and one of those will contain the ordinal.
  // This message comes to us before the final update is sent.
  if (find_details.active_match_ordinal() > -1) {
    active_match_ordinal_ = find_details.active_match_ordinal();
    selection_rect_ = find_details.selection_rect();
  }
  if (find_details.final_update()) {
    number_of_matches_ = find_details.number_of_matches();
    seen_ = true;
    if (run_loop_ && run_loop_->running())
      run_loop_->Quit();
  } else {
    DVLOG(1) << "Ignoring, since we only care about the final message";
  }
}

}  // namespace ui_test_utils
