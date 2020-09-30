// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONTROL_SERVICE_IN_PROCESS_H_
#define CHROME_UPDATER_CONTROL_SERVICE_IN_PROCESS_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/control_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace updater {

class Configurator;
class PersistedData;

// All functions and callbacks must be called on the same sequence.
class ControlServiceInProcess : public ControlService {
 public:
  explicit ControlServiceInProcess(scoped_refptr<updater::Configurator> config);

  // Overrides for updater::ControlService.
  void Run(base::OnceClosure callback) override;
  void InitializeUpdateService(base::OnceClosure callback) override;

  void Uninitialize() override;

 private:
  ~ControlServiceInProcess() override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Checks for updates of all registered applications if it has been longer
  // than the last check time by NextCheckDelay() amount defined in the config.
  void MaybeCheckForUpdates(base::OnceClosure callback);

  // Provides a way to remove apps from the persisted data if the app is no
  // longer installed on the machine.
  void UnregisterMissingApps();

  scoped_refptr<updater::Configurator> config_;
  scoped_refptr<updater::PersistedData> persisted_data_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CONTROL_SERVICE_IN_PROCESS_H_
