// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"
#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/engines/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_cleaner.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade.h"
#include "chrome/chrome_cleaner/engines/controllers/scanner_impl.h"
#include "chrome/chrome_cleaner/engines/controllers/uwe_engine_cleaner_wrapper.h"
#include "chrome/chrome_cleaner/engines/controllers/uwe_scanner_wrapper.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner_impl.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"

namespace chrome_cleaner {

EngineFacade::EngineFacade(scoped_refptr<EngineClient> engine_client,
                           JsonParserAPI* json_parser,
                           MainDialogAPI* main_dialog,
                           std::unique_ptr<ForceInstalledExtensionScanner>
                               force_installed_extension_scanner,
                           ChromePromptIPC* chrome_prompt_ipc)
    : engine_client_(std::move(engine_client)),
      force_installed_extension_scanner_(
          std::move(force_installed_extension_scanner)),
      uwe_matchers_(
          force_installed_extension_scanner_->CreateUwEMatchersFromResource(
              GetUwEMatchersResourceID())),
      main_dialog_(main_dialog) {
  CHECK(sandbox::SandboxFactory::GetTargetServices() == nullptr);

  std::vector<ForceInstalledExtension> force_installed_extensions =
      force_installed_extension_scanner_->GetForceInstalledExtensions(
          json_parser);

  scanner_ = std::make_unique<UwEScannerWrapper>(
      std::make_unique<ScannerImpl>(engine_client_.get()), uwe_matchers_.get(),
      std::move(force_installed_extensions));
  cleaner_ = std::make_unique<UwEEngineCleanerWrapper>(
      std::make_unique<EngineCleaner>(engine_client_.get()),
      base::BindOnce(&MainDialogAPI::DisableExtensions,
                     base::Unretained(main_dialog_)),
      chrome_prompt_ipc);
}

EngineFacade::~EngineFacade() = default;

Scanner* EngineFacade::GetScanner() {
  return scanner_.get();
}

Cleaner* EngineFacade::GetCleaner() {
  return cleaner_.get();
}

base::TimeDelta EngineFacade::GetScanningWatchdogTimeout() const {
  return base::TimeDelta::FromSeconds(
      engine_client_->ScanningWatchdogTimeoutInSeconds());
}

}  // namespace chrome_cleaner
