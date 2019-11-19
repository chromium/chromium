// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ELEVATING_FACADE_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ELEVATING_FACADE_H_

#include <memory>

#include "chrome/chrome_cleaner/engines/controllers/engine_facade_interface.h"

namespace chrome_cleaner {

// Facade that will provide cleaner that starts unprivileged and spawns a
// process with elevated privileges in order to complete cleanup.
class ElevatingFacade : public EngineFacadeInterface {
 public:
  explicit ElevatingFacade(std::unique_ptr<EngineFacadeInterface> real_facade);
  ~ElevatingFacade() override;

  Scanner* GetScanner() override;
  Cleaner* GetCleaner() override;

  base::TimeDelta GetScanningWatchdogTimeout() const override;

 public:
  std::unique_ptr<EngineFacadeInterface> decorated_facade_;
  std::unique_ptr<Cleaner> cleaner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ELEVATING_FACADE_H_
