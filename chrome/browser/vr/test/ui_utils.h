// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_UTILS_H_
#define CHROME_BROWSER_VR_TEST_UI_UTILS_H_

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

class BrowserRenderer;
class BrowserTestBrowserRendererBrowserInterface;
class VRBrowserRendererThreadWin;

// Port of the equivalent NativeUiUtils.java for instrumentation tests. Contains
// utility functions for interacting with the native VR UI, e.g. notifications
// shown in the headset.
class UiUtils {
 public:
  static constexpr int kDefaultUiQuiescenceTimeout = 2000;

  // Ensures that the renderer thread and BrowserRenderer instance exist before
  // creating a UiUtils.
  // TODO(https://crbug.com/920697): Remove this once the BrowserRenderer's
  // lifetime is tied to the renderer thread and we can assume that they both
  // exist if we're in VR.
  static std::unique_ptr<UiUtils> Create();

  UiUtils();
  ~UiUtils();

  // Runs |action| and waits until the native UI reports that |element_name|'s
  // visibility matches |visible|. Fails if the visibility is not matched in
  // an allotted amount of time.
  void PerformActionAndWaitForVisibilityStatus(
      const UserFriendlyElementName& element_name,
      const bool& visible,
      base::OnceCallback<void()> action);

  // Not meant to be called directly by a test.
  void ReportUiOperationResult(const UiTestOperationType& action_type,
                               const UiTestOperationResult& result);

  static void DisableFrameTimeoutForTesting();

 private:
  static void PollForBrowserRenderer(base::RunLoop* wait_loop);
  static VRBrowserRendererThreadWin* GetRendererThread();
  static BrowserRenderer* GetBrowserRenderer();

  void WatchElementForVisibilityStatusForTesting(
      VisibilityChangeExpectation visibility_expectation);
  std::string UiTestOperationResultToString(UiTestOperationResult& result);

  std::unique_ptr<BrowserTestBrowserRendererBrowserInterface> interface_;
  std::vector<UiTestOperationResult> ui_operation_results_;
  std::vector<base::OnceCallback<void()>> ui_operation_callbacks_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UiUtils);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_UTILS_H_
