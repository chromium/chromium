// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SYSTEM_REBOOT_FUCHSIA_COMPONENT_RESTART_REASON_H_
#define CHROMECAST_SYSTEM_REBOOT_FUCHSIA_COMPONENT_RESTART_REASON_H_

#include "base/files/file_path.h"
#include "chromecast/public/reboot_shlib.h"

namespace chromecast {

// This class tracks the restart reason of the Cast component using temp files
// to indicate whether it restarts gracefully or ungracefully. On Fuchsia, the
// Cast component can restart according to plan, or by platform request,
// without the need for the device to reboot.
// No more than one instance of this class variable should be used per component
// to guarantee the correctness of the restart reason.
//
// The caller/user of this class must provide isolated-temp feature in sandbox.
class FuchsiaComponentRestartReason {
 public:
  FuchsiaComponentRestartReason();

  // Whether the component restarted and if so, for which reason (graceful or
  // ungraceful).
  bool GetRestartReason(RebootShlib::RebootSource* restart_reason);

  // Registers a graceful teardown of the component so we can distinguish
  // between the first start of a boot cycle and a restart during the same
  // boot cycle.
  void RegisterTeardown();

  // This can be called once during initialization (not necessary).
  // But is necessary during testing if SetUp is shared.
  void ResetRestartCheck();

  // Change tmp folder path for testing purpose.
  // Not necessary for production use.
  const base::FilePath& SetFlagFileDirForTesting(const base::FilePath& sub);

 private:
  // To change tmp directory for testing purpose when run in parallel.
  base::FilePath tmp_dir_;

  bool restart_checked_ = false;
  bool was_restart_ = true;
  RebootShlib::RebootSource restart_reason_{RebootShlib::RebootSource::UNKNOWN};
};

}  // namespace chromecast

#endif  // CHROMECAST_SYSTEM_REBOOT_FUCHSIA_COMPONENT_RESTART_REASON_H_
