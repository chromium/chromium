// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade_interface.h"

namespace chrome_cleaner {

class EngineFacade : public EngineFacadeInterface {
 public:
  explicit EngineFacade(scoped_refptr<EngineClient> engine_client);
  ~EngineFacade() override;

  Scanner* GetScanner() override;
  Cleaner* GetCleaner() override;
  base::TimeDelta GetScanningWatchdogTimeout() const override;

 private:
  // This must be declared before |scanner_| and |cleaner_| so that it is
  // deleted after them, since they hold raw pointers to it.
  scoped_refptr<EngineClient> engine_client_;

  std::unique_ptr<Scanner> scanner_;
  std::unique_ptr<Cleaner> cleaner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_H_
