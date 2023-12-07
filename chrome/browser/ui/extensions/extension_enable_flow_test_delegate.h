// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_TEST_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_TEST_DELEGATE_H_

#include <optional>

#include "base/run_loop.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"

class ExtensionEnableFlowTestDelegate : public ExtensionEnableFlowDelegate {
 public:
  ExtensionEnableFlowTestDelegate();
  ~ExtensionEnableFlowTestDelegate() override;
  ExtensionEnableFlowTestDelegate(const ExtensionEnableFlowTestDelegate&) =
      delete;
  ExtensionEnableFlowTestDelegate& operator=(
      const ExtensionEnableFlowTestDelegate&) = delete;

  enum Result {
    ABORTED,
    FINISHED,
  };

  // ExtensionEnableFlowDelegate:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // Wait for the extension enable flow to complete.
  void Wait();

  const std::optional<Result>& result() const { return result_; }

 private:
  std::optional<Result> result_;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_TEST_DELEGATE_H_
