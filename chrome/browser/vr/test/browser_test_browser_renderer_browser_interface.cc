// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/browser_test_browser_renderer_browser_interface.h"
#include "base/functional/bind.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/test/ui_utils.h"

namespace vr {

BrowserTestBrowserRendererBrowserInterface::
    BrowserTestBrowserRendererBrowserInterface(UiUtils* utils)
    : utils_(utils) {}

BrowserTestBrowserRendererBrowserInterface::
    ~BrowserTestBrowserRendererBrowserInterface() = default;

void BrowserTestBrowserRendererBrowserInterface::ForceExitVr() {}

void BrowserTestBrowserRendererBrowserInterface::
    ReportUiOperationResultForTesting(const UiTestOperationType& action_type,
                                      const UiTestOperationResult& result) {
  utils_->ReportUiOperationResult(action_type, result);
}

}  // namespace vr
