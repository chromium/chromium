// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade_interface.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/proto/uwe_matcher.pb.h"
#include "chrome/chrome_cleaner/scanner/force_installed_extension_scanner.h"
#include "chrome/chrome_cleaner/ui/main_dialog_api.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

class EngineFacade : public EngineFacadeInterface {
 public:
  EngineFacade(scoped_refptr<EngineClient> engine_client,
               JsonParserAPI* json_parser,
               MainDialogAPI* main_dialog,
               std::unique_ptr<ForceInstalledExtensionScanner>
                   force_installed_extension_scanner,
               ChromePromptIPC* chrome_prompt_ipc = nullptr);
  ~EngineFacade() override;

  Scanner* GetScanner() override;
  Cleaner* GetCleaner() override;
  base::TimeDelta GetScanningWatchdogTimeout() const override;

 private:
  base::string16 interface_log_file_;

  // This must be declared before |scanner_| and |cleaner_| so that it is
  // deleted after them, since they hold raw pointers to it.
  scoped_refptr<EngineClient> engine_client_;

  std::unique_ptr<Scanner> scanner_;
  std::unique_ptr<Cleaner> cleaner_;
  // API to get force installed extensions.
  std::unique_ptr<ForceInstalledExtensionScanner>
      force_installed_extension_scanner_;
  // List of pre-compiled matches to find UwE.
  std::unique_ptr<UwEMatchers> uwe_matchers_;

  MainDialogAPI* main_dialog_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_FACADE_H_
