// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_INSPECTOR_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_INSPECTOR_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/timer/timer.h"
#include "chrome/browser/win/conflicts/inspection_results_cache.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}

// This class takes care of inspecting several modules (identified by their
// ModuleInfoKey) and returning the result via the OnModuleInspectedCallback on
// the SequencedTaskRunner where it was created.
//
// The inspection of all modules is quite expensive in terms of resources, so it
// is done one by one, in a utility process, only if it is needed (when
// StartInspection() is called).
//
// This class is not thread safe and it enforces safety via a SEQUENCE_CHECKER.
class ModuleInspector : public ModuleDatabaseObserver {
 public:
  // The amount of time before the |inspection_results_cache_| is flushed to
  // disk while the ModuleDatabase is not idle.
  static constexpr base::TimeDelta kFlushInspectionResultsTimerTimeout =
      base::Minutes(5);

  using OnModuleInspectedCallback =
      base::RepeatingCallback<void(const ModuleInfoKey& module_key,
                                   ModuleInspectionResult inspection_result)>;

  explicit ModuleInspector(
      const OnModuleInspectedCallback& on_module_inspected_callback);

  ModuleInspector(const ModuleInspector&) = delete;
  ModuleInspector& operator=(const ModuleInspector&) = delete;

  ~ModuleInspector() override;

  // Starts the background inspection of modules.
  void StartInspection();

  // Adds the module to the queue of modules to inspect. Starts the inspection
  // process if the |queue_| is empty.
  void AddModule(const ModuleInfoKey& module_key);

  // Returns true if ModuleInspector is not doing anything right now.
  bool IsIdle();

  // ModuleDatabaseObserver:
  void OnModuleDatabaseIdle() override;

  static base::FilePath GetInspectionResultsCachePath();

  // Sets a test factory function to create the UtilWin instance.
  using UtilWinFactoryCallback =
      base::RepeatingCallback<mojo::Remote<chrome::mojom::UtilWin>()>;
  void SetUtilWinFactoryCallbackForTesting(
      UtilWinFactoryCallback util_win_factory_callback);

  int get_connection_error_retry_count_for_testing() const {
    return connection_error_retry_count_;
  }

 private:
  // Ensures the |remote_util_win_| instance is bound to the UtilWin service.
  void EnsureUtilWinServiceBound();

  // Invoked when the InspectionResultsCache is available.
  void OnInspectionResultsCacheRead(
      InspectionResultsCache inspection_results_cache);

  // Handles a connection error to the UtilWin service.
  void OnUtilWinServiceConnectionError();

  // Starts inspecting the module at the front of the queue.
  void StartInspectingModule();

  // Adds the newly inspected module to the cache then calls
  // OnInspectionFinished().
  void OnModuleNewlyInspected(const ModuleInfoKey& module_key,
                              ModuleInspectionResult inspection_result);

  // Called back on the execution context on which the ModuleInspector was
  // created when a module has finished being inspected. The callback will be
  // executed and, if the |queue_| is not empty, the next module will be sent
  // for inspection.
  void OnInspectionFinished(const ModuleInfoKey& module_key,
                            ModuleInspectionResult inspection_result);

  // Sends a task on a blocking background sequence to serialize
  // |inspection_results_cache_|, should it be needed.
  void MaybeUpdateInspectionResultsCache();

  OnModuleInspectedCallback on_module_inspected_callback_;

  // The modules are put in queue until they are sent for inspection.
  base::queue<ModuleInfoKey> queue_;

  // Indicates inspection was started. Used to delay the background inspection
  // tasks until the results have been requested (by calling StartInspection()).
  bool is_started_;

  // A callback used to initialize |remote_util_win_|.
  UtilWinFactoryCallback util_win_factory_callback_;

  // A remote interface to the UtilWin service. It is created when inspection is
  // ongoing, and freed when no longer needed.
  mojo::Remote<chrome::mojom::UtilWin> remote_util_win_;

  // The vector of paths to %env_var%, used to account for differences in
  // localization and where people keep their files.
  // e.g. c:\windows vs d:\windows
  StringMapping path_mapping_;

  // This task runner handles updates to the inspection results cache.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // Indicates if the inspection results cache was read from disk.
  bool inspection_results_cache_read_;

  // Contains the cached inspection results so that a module is not inspected
  // more than once between restarts.
  InspectionResultsCache inspection_results_cache_;

  // Ensures that newly inspected modules are flushed to the disk after at most
  // 5 minutes to avoid losing too much of the work done if the browser is
  // closed before all modules are inspected.
  base::RetainingOneShotTimer flush_inspection_results_timer_;

  // Indicates if a module was newly inspected and the cache must be updated.
  bool has_new_inspection_results_;

  // The number of time this class will try to restart the UtilWin service if a
  // connection error occurs. This is to prevent the degenerate case where the
  // service always fails to start and the restart cycle happens infinitely.
  int connection_error_retry_count_;

  // Indicates if a module is currently being inspected asynchronously by the
  // UtilWin service.
  bool is_waiting_on_util_win_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers are used to safely post the inspection result back to the
  // ModuleInspector from the task scheduler.
  base::WeakPtrFactory<ModuleInspector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_INSPECTOR_H_
