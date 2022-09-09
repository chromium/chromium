// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/engine_facade.h"

#include <utility>

#include "base/check.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_cleaner.h"
#include "chrome/chrome_cleaner/engines/controllers/scanner_impl.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"

namespace chrome_cleaner {

EngineFacade::EngineFacade(scoped_refptr<EngineClient> engine_client)
    : engine_client_(std::move(engine_client)) {
  CHECK(sandbox::SandboxFactory::GetTargetServices() == nullptr);

  scanner_ = std::make_unique<ScannerImpl>(engine_client_.get());
  cleaner_ = std::make_unique<EngineCleaner>(engine_client_.get());
}

EngineFacade::~EngineFacade() = default;

Scanner* EngineFacade::GetScanner() {
  return scanner_.get();
}

Cleaner* EngineFacade::GetCleaner() {
  return cleaner_.get();
}

base::TimeDelta EngineFacade::GetScanningWatchdogTimeout() const {
  return base::Seconds(engine_client_->ScanningWatchdogTimeoutInSeconds());
}

}  // namespace chrome_cleaner
