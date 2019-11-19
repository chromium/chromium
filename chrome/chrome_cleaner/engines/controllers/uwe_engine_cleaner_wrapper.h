// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_UWE_ENGINE_CLEANER_WRAPPER_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_UWE_ENGINE_CLEANER_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/cleaner/cleaner.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// The callback triggered when we need to tell Chrome to disable |extensions|.
// Currently this is only triggered for extensions installed through master
// preferences. The |on_done| callback is triggered once this callback has
// completed and can be used for error reporting.
typedef base::OnceCallback<void(const std::vector<base::string16>& extensions,
                                base::OnceCallback<void(bool)> on_done)>
    DisableExtensionsCallback;

// Removes found UwE associated with known UwS before starting
// the wrapped engine cleaner.
// Delegates all other calls to the wrapped engine cleaner.
class UwEEngineCleanerWrapper : public Cleaner {
 public:
  UwEEngineCleanerWrapper(std::unique_ptr<Cleaner> cleaner,
                          DisableExtensionsCallback disable_extensions_callback,
                          ChromePromptIPC* chrome_prompt_ipc = nullptr);
  ~UwEEngineCleanerWrapper() override;

  void DisableExtensionDone(bool result);

  // Cleaner implementation.

  // Start cleaning
  void Start(const std::vector<UwSId>& pup_ids,
             DoneCallback done_callback) override;

  void StartPostReboot(const std::vector<UwSId>& pup_ids,
                       DoneCallback done_callback) override;

  void Stop() override;

  bool IsCompletelyDone() const override;

  bool CanClean(const std::vector<UwSId>& pup_ids) override;

 private:
  void TryRemovePUPExtensions(const std::vector<UwSId>& pup_ids);

  void RemovePUPExtensions(const std::vector<UwSId>& pup_ids);

  void OnDoneUwSCleanup(ResultCode status);

  // Called once OnDoneUwSCleanup is called by the wrapped engine cleaner
  // and once the PUP Extension removal task has completed.
  // Will post done_callback_ to |task_runner|.
  void OnTotallyDone(scoped_refptr<base::SequencedTaskRunner> task_runner);

  std::unique_ptr<Cleaner> cleaner_;
  std::vector<UwSId> pups_to_clean_;
  DoneCallback done_callback_;
  ResultCode extension_removal_result_;
  ResultCode uws_removal_result_;
  base::RepeatingClosure task_barrier_closure_;
  bool is_totally_done_;
  DisableExtensionsCallback disable_extensions_callback_;
  ChromePromptIPC* chrome_prompt_ipc_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_UWE_ENGINE_CLEANER_WRAPPER_H_
