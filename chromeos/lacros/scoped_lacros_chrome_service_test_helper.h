// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_SCOPED_LACROS_CHROME_SERVICE_TEST_HELPER_H_
#define CHROMEOS_LACROS_SCOPED_LACROS_CHROME_SERVICE_TEST_HELPER_H_

#include <memory>

#include "base/component_export.h"

namespace chromeos {

class LacrosChromeServiceImpl;

// Helper for teststo instantiate LacrosChromeServiceImpl. This should only be
// used for unit tests, not browser tests.
class COMPONENT_EXPORT(CHROMEOS_LACROS) ScopedLacrosChromeServiceTestHelper {
 public:
  ScopedLacrosChromeServiceTestHelper();
  virtual ~ScopedLacrosChromeServiceTestHelper();

  ScopedLacrosChromeServiceTestHelper(
      const ScopedLacrosChromeServiceTestHelper&) = delete;
  ScopedLacrosChromeServiceTestHelper& operator=(
      const ScopedLacrosChromeServiceTestHelper&) = delete;

 private:
  std::unique_ptr<LacrosChromeServiceImpl> lacros_chrome_service_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_SCOPED_LACROS_CHROME_SERVICE_TEST_HELPER_H_
