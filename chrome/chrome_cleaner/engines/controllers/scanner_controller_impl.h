// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_SCANNER_CONTROLLER_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_SCANNER_CONTROLLER_IMPL_H_

#include <set>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/engines/controllers/scanner_impl.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/scanner/scanner_controller.h"

namespace chrome_cleaner {

// The sandboxed implementation of the ScannerController.
class ScannerControllerImpl : public ScannerController {
 public:
  explicit ScannerControllerImpl(
      EngineClient* engine_client,
      RegistryLogger* registry_logger,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ShortcutParserAPI* shortcut_parser);
  // If |StartScan| has been called, pumps the message loop until
  // |HandleScanDone| is called.
  ~ScannerControllerImpl() override;

 protected:
  // ScannerController:
  void StartScan() override;
  int WatchdogTimeoutCallback() override;

 private:
  void OnFoundUwS(UwSId pup_id);
  void OnScanDone(ResultCode result_code, const std::vector<UwSId>& found_uws);

  void UpdateResultsOnFoundUwS(UwSId pup_id);
  void HandleScanDone(ResultCode result, const std::vector<UwSId>& found_uws);

  bool IsScanningInProgress() const;

  ScannerImpl scanner_;
  EngineClient* engine_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  enum class State {
    kIdle,
    kScanningStarting,
    kScanningInProgress,
    kScanningFinishing,
  };
  State state_ = State::kIdle;

  // TODO(veranika): This is getting out of hand. Now there are two of them.
  // We should have only one source of truth for the list of UwS found, and
  // this list is also kept by the scanner.
  std::set<UwSId> pup_ids_;

  DISALLOW_COPY_AND_ASSIGN(ScannerControllerImpl);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_SCANNER_CONTROLLER_IMPL_H_
