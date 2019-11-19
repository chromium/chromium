// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_SCANNER_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_SCANNER_IMPL_H_

#include <set>
#include <string>

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/scanner/scanner.h"

namespace chrome_cleaner {

class ScannerImpl : public Scanner {
 public:
  // Passed |engine_client| should stay alive the entire lifetime of the object.
  explicit ScannerImpl(EngineClient* engine_client);
  ~ScannerImpl() override;

  // Scanner implementation.

  // Start scanning.
  bool Start(const FoundUwSCallback& found_uws_callback,
             DoneCallback done_callback) override;

  void Stop() override;

  // When calling |Stop|, the engine may still be running, so make sure to call
  // |IsCompletelyDone| and allow the main UI to pump messages to let the engine
  // finish gracefully.
  bool IsCompletelyDone() const override;

 private:
  // Sandboxed client callbacks.
  void OnFoundUwS(UwSId pup_id, const PUPData::PUP& pup);
  void OnScanDone(uint32_t result);

  void HandleFoundUwS(UwSId pup_id, const PUPData::PUP& pup);
  void HandleScanDone(uint32_t result);

  EngineClient* engine_client_;

  std::set<UwSId> found_uws_;

  FoundUwSCallback found_uws_callback_;
  DoneCallback done_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool is_scanning_ = false;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_SCANNER_IMPL_H_
