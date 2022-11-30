// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow_test_delegate.h"

ExtensionEnableFlowTestDelegate::ExtensionEnableFlowTestDelegate() = default;
ExtensionEnableFlowTestDelegate::~ExtensionEnableFlowTestDelegate() = default;

void ExtensionEnableFlowTestDelegate::ExtensionEnableFlowFinished() {
  result_ = FINISHED;
  run_loop_.Quit();
}

void ExtensionEnableFlowTestDelegate::ExtensionEnableFlowAborted(
    bool user_initiated) {
  result_ = ABORTED;
  run_loop_.Quit();
}

void ExtensionEnableFlowTestDelegate::Wait() {
  run_loop_.Run();
}
