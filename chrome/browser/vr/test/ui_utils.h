// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_UTILS_H_
#define CHROME_BROWSER_VR_TEST_UI_UTILS_H_

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

class BrowserRenderer;
class VRBrowserRendererThread;

// Port of the equivalent NativeUiUtils.java for instrumentation tests. Contains
// utility functions for interacting with the native VR UI, e.g. notifications
// shown in the headset.
class UiUtils {
 public:
  static constexpr int kDefaultUiQuiescenceTimeout = 2000;

  // Ensures that the renderer thread and BrowserRenderer instance exist before
  // creating a UiUtils.
  // TODO(crbug.com/41434932): Remove this once the BrowserRenderer's
  // lifetime is tied to the renderer thread and we can assume that they both
  // exist if we're in VR.
  static std::unique_ptr<UiUtils> Create();

  UiUtils();

  UiUtils(const UiUtils&) = delete;
  UiUtils& operator=(const UiUtils&) = delete;

  ~UiUtils();

  // Waits until the native UI reports that |element_name|'s visibility matches
  // |visible|. Fails if the visibility is not matched in an allotted amount of
  // time.
  void WaitForVisibilityStatus(const UserFriendlyElementName& element_name,
                               const bool& visible);

  static void DisableOverlayForTesting();

 private:
  static void PollForBrowserRenderer(base::RunLoop* wait_loop);
  static VRBrowserRendererThread* GetRendererThread();
  static BrowserRenderer* GetBrowserRenderer();

  void WatchElementForVisibilityStatusForTesting(
      std::optional<UiVisibilityState> visibility_expectation);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_UTILS_H_
