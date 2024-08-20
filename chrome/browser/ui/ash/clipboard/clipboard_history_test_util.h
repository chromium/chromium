// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_TEST_UTIL_H_

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/clipboard/clipboard_image_model_request.h"

namespace clipboard_history {

// The helper class to wait for the completion of the image model request.
class ClipboardImageModelRequestWaiter {
 public:
  ClipboardImageModelRequestWaiter(
      ClipboardImageModelRequest::TestParams* test_params,
      bool expect_auto_resize);
  ClipboardImageModelRequestWaiter(const ClipboardImageModelRequestWaiter&) =
      delete;
  ClipboardImageModelRequestWaiter& operator=(
      const ClipboardImageModelRequestWaiter&) = delete;
  ~ClipboardImageModelRequestWaiter();

  void Wait();

  void OnRequestStop(bool use_auto_resize_mode);

 private:
  const raw_ptr<ClipboardImageModelRequest::TestParams> test_params_;
  const bool expect_auto_resize_;

  base::RunLoop run_loop_;
};

// The helper class to wait for an update to the clipboard history item list.
class ScopedClipboardHistoryListUpdateWaiter
    : public ash::ClipboardHistoryController::Observer {
 public:
  ScopedClipboardHistoryListUpdateWaiter();
  ScopedClipboardHistoryListUpdateWaiter(
      const ScopedClipboardHistoryListUpdateWaiter&) = delete;
  ScopedClipboardHistoryListUpdateWaiter& operator=(
      const ScopedClipboardHistoryListUpdateWaiter&) = delete;
  ~ScopedClipboardHistoryListUpdateWaiter() override;

  // ash::ClipboardHistoryController::Observer:
  void OnClipboardHistoryItemsUpdated() override;

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ash::ClipboardHistoryController,
                          ash::ClipboardHistoryController::Observer>
      controller_observation_{this};
};

}  // namespace clipboard_history

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_TEST_UTIL_H_
