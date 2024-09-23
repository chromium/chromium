// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard/clipboard_history_test_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace clipboard_history {

// ClipboardImageModelRequestWaiter --------------------------------------------

ClipboardImageModelRequestWaiter::ClipboardImageModelRequestWaiter(
    ClipboardImageModelRequest::TestParams* test_params,
    bool expect_auto_resize)
    : test_params_(test_params), expect_auto_resize_(expect_auto_resize) {
  test_params_->callback = base::BindRepeating(
      &ClipboardImageModelRequestWaiter::OnRequestStop, base::Unretained(this));
  ClipboardImageModelRequest::SetTestParams(test_params_);
}

ClipboardImageModelRequestWaiter::~ClipboardImageModelRequestWaiter() {
  test_params_->callback = base::NullCallback();
  ClipboardImageModelRequest::SetTestParams(nullptr);
}

void ClipboardImageModelRequestWaiter::Wait() {
  run_loop_.Run();
}

void ClipboardImageModelRequestWaiter::OnRequestStop(
    bool use_auto_resize_mode) {
  EXPECT_EQ(expect_auto_resize_, use_auto_resize_mode);
  run_loop_.Quit();
}

// ScopedClipboardHistoryListUpdateWaiter --------------------------------------

ScopedClipboardHistoryListUpdateWaiter::
    ScopedClipboardHistoryListUpdateWaiter() {
  controller_observation_.Observe(ash::ClipboardHistoryController::Get());
}

ScopedClipboardHistoryListUpdateWaiter::
    ~ScopedClipboardHistoryListUpdateWaiter() {
  run_loop_.Run();
}

void ScopedClipboardHistoryListUpdateWaiter::OnClipboardHistoryItemsUpdated() {
  run_loop_.Quit();
}

}  // namespace clipboard_history
