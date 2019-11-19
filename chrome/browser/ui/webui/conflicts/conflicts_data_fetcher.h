// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_DATA_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_DATA_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#endif

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

// This class is responsible for gathering the list of modules for the
// chrome://conflicts page and the state of the third-party features on the
// ModuleDatabase task runner and sending it back to the UI thread. The instance
// should be deleted once the OnConflictsDataFetchedCallback is invoked.
class ConflictsDataFetcher : public ModuleDatabaseObserver {
 public:
  using UniquePtr =
      std::unique_ptr<ConflictsDataFetcher, base::OnTaskRunnerDeleter>;
  using OnConflictsDataFetchedCallback =
      base::OnceCallback<void(base::DictionaryValue results)>;

  ~ConflictsDataFetcher() override;

  // Creates the instance and initializes it on the ModuleDatabase task runner.
  // |on_conflicts_data_fetched_callback| will be invoked on the caller's
  // sequence.
  static UniquePtr Create(
      OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback);

 private:
  explicit ConflictsDataFetcher(
      OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback);

  void InitializeOnModuleDatabaseTaskRunner();

#if defined(GOOGLE_CHROME_BUILD)
  // Invoked when the ThirdPartyConflictsManager initialization state is
  // available.
  void OnManagerInitializationComplete(ThirdPartyConflictsManager::State state);
#endif

  // Registers this instance to the ModuleDatabase to retrieve the list of
  // modules via the ModuleDatabaseObserver API.
  void GetListOfModules();

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

  OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback_;

  // Temporarily holds the module list while the modules are being
  // enumerated.
  std::unique_ptr<base::ListValue> module_list_;

  SEQUENCE_CHECKER(sequence_checker_);

#if defined(GOOGLE_CHROME_BUILD)
  base::Optional<ThirdPartyConflictsManager::State>
      third_party_conflicts_manager_state_;

  base::WeakPtrFactory<ConflictsDataFetcher> weak_ptr_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ConflictsDataFetcher);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_DATA_FETCHER_H_
