// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_INTERFACE_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_INTERFACE_H_

#include "base/time/time.h"
#include "chrome/chrome_cleaner/cleaner/cleaner.h"
#include "chrome/chrome_cleaner/scanner/scanner.h"

namespace chrome_cleaner {

// Interface for classes that provide access to engine-specific cleaner and
// reporter. The subclasses are expected to own their cleaner and reporter.
class EngineFacadeInterface {
 public:
  virtual ~EngineFacadeInterface() = default;
  virtual Scanner* GetScanner() = 0;
  virtual Cleaner* GetCleaner() = 0;
  virtual base::TimeDelta GetScanningWatchdogTimeout() const = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_INTERFACE_H_
