// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_CLEANER_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_CLEANER_H_

#include <vector>

#include "base/sequence_checker.h"
#include "chrome/chrome_cleaner/cleaner/cleaner.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

class EngineCleaner : public Cleaner {
 public:
  // Passed |engine_client| should stay alive the entire lifetime of the object.
  explicit EngineCleaner(EngineClient* engine_client);
  ~EngineCleaner() override;

  // Cleaner implementation.

  // Start cleaning.
  void Start(const std::vector<UwSId>& pup_ids,
             DoneCallback done_callback) override;

  void StartPostReboot(const std::vector<UwSId>& pup_ids,
                       DoneCallback done_callback) override;

  void Stop() override;

  bool IsCompletelyDone() const override;

  bool CanClean(const std::vector<UwSId>& pup_ids) override;

 private:
  void OnCleanupDone(uint32_t result);

  void HandleCleanupDone(uint32_t result);

  EngineClient* engine_client_;

  DoneCallback done_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool is_cleaning_ = false;
  std::vector<UwSId> pups_to_clean_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_ENGINE_CLEANER_H_
