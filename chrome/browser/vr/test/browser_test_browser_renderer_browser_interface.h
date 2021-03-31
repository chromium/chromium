// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_BROWSER_TEST_BROWSER_RENDERER_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_VR_TEST_BROWSER_TEST_BROWSER_RENDERER_BROWSER_INTERFACE_H_

#include "base/single_thread_task_runner.h"
#include "chrome/browser/vr/browser_renderer_browser_interface.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

class UiUtils;

class BrowserTestBrowserRendererBrowserInterface
    : public BrowserRendererBrowserInterface {
 public:
  explicit BrowserTestBrowserRendererBrowserInterface(UiUtils* utils);
  ~BrowserTestBrowserRendererBrowserInterface() override;

  // BrowserRendererBrowserInterface
  void ForceExitVr() override;
  void ReportUiOperationResultForTesting(
      const UiTestOperationType& action_type,
      const UiTestOperationResult& result) override;

 private:
  // Should not have to worry about the lifetime of this, as this should be a
  // reference to the UiUtils that created this
  // BrowserTestBrowserRendererBrowserInterface, and the interface should always
  // be destroyed before the utils.
  UiUtils* utils_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_BROWSER_TEST_BROWSER_RENDERER_BROWSER_INTERFACE_H_
