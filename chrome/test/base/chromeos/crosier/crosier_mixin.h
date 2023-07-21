// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CROSIER_MIXIN_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CROSIER_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

// Any crosier tests should use CrosierMixin test helper which
// provides the framework setup.
class CrosierMixin : public InProcessBrowserTestMixin {
 public:
  explicit CrosierMixin(InProcessBrowserTestMixinHost* host)
      : InProcessBrowserTestMixin(host) {}
  CrosierMixin(const CrosierMixin&) = delete;
  CrosierMixin& operator=(const CrosierMixin&) = delete;
  ~CrosierMixin() override = default;

  // InProcessBrowserTestMixin:
  bool SetUpUserDataDirectory() override;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CROSIER_MIXIN_H_
