// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_RESULT_CODES_H_
#define CHROME_COMMON_CHROME_RESULT_CODES_H_

#include "content/public/common/result_codes.h"

namespace chrome {

// IMPORTANT: This needs to stay in sync with <enum name="CrashExitCodes"> and
// <enum name="WindowsExitCode"> in tools/metrics/histograms/enums.xml. So do
// not remove any entries, and always append entries to the bottom just above
// RESULT_CODE_CHROME_LAST_CODE.

enum ResultCode {
  RESULT_CODE_CHROME_START = content::RESULT_CODE_LAST_CODE,

  // An invalid command line url was given.
  RESULT_CODE_INVALID_CMDLINE_URL = RESULT_CODE_CHROME_START,

  // The process is of an unknown type.
  RESULT_CODE_BAD_PROCESS_TYPE,

  // A critical chrome file is missing.
  RESULT_CODE_MISSING_DATA,

  // Failed to make Chrome default browser (not used?).
  RESULT_CODE_SHELL_INTEGRATION_FAILED,

  // Machine level install exists
  RESULT_CODE_MACHINE_LEVEL_INSTALL_EXISTS,

  // Uninstall detected another chrome instance.
  RESULT_CODE_UNINSTALL_CHROME_ALIVE,

  // The user changed their mind.
  RESULT_CODE_UNINSTALL_USER_CANCEL,

  // Delete profile as well during uninstall.
  RESULT_CODE_UNINSTALL_DELETE_PROFILE,

  // Command line parameter is not supported.
  RESULT_CODE_UNSUPPORTED_PARAM,

  // Browser import hung and was killed.
  RESULT_CODE_IMPORTER_HUNG,

  // Trying to restart the browser we crashed.
  RESULT_CODE_RESPAWN_FAILED,

  // The EXP1, EXP2, EXP3, EXP4 are generic codes used to communicate some
  // simple outcome back to the process that launched us. This is used for
  // experiments and the actual meaning depends on the experiment.
  // (only EXP2 is used?)
  RESULT_CODE_NORMAL_EXIT_EXP1,
  RESULT_CODE_NORMAL_EXIT_EXP2,
  RESULT_CODE_NORMAL_EXIT_EXP3,
  RESULT_CODE_NORMAL_EXIT_EXP4,

  // For experiments this return code means that the user canceled causes the
  // did_run "dr" signal to be reset soi this chrome run does not count as
  // active chrome usage.
  RESULT_CODE_NORMAL_EXIT_CANCEL,

  // The profile was in use on another host.
  RESULT_CODE_PROFILE_IN_USE,

  // Failed to pack an extension via the cmd line.
  RESULT_CODE_PACK_EXTENSION_ERROR,

  // Failed to silently uninstall an extension.
  RESULT_CODE_UNINSTALL_EXTENSION_ERROR,

  // The browser process exited early by passing the command line to another
  // running browser.
  RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED,

  // A dummy value we should not use. See crbug.com/152285.
  RESULT_CODE_NOTUSED_1,

  // Failed to install an item from the webstore when the
  // kInstallEphemeralAppFromWebstore command line flag was present.
  // As this flag is no longer supported, this return code should never be
  // returned.
  RESULT_CODE_INSTALL_FROM_WEBSTORE_ERROR_2,

  // A dummy value we should not use. See crbug.com/152285.
  RESULT_CODE_NOTUSED_2,

  // Returned when the user has not yet accepted the EULA.
  RESULT_CODE_EULA_REFUSED,

  // Failed to migrate user data directory for side-by-side package support
  // (Linux-only).
  RESULT_CODE_SXS_MIGRATION_FAILED_NOT_USED,

  // The action is not allowed by a policy.
  RESULT_CODE_ACTION_DISALLOWED_BY_POLICY,

  // An browser process was sandboxed. This should never happen.
  RESULT_CODE_INVALID_SANDBOX_STATE,

  // Cloud policy enrollment is failed or given up by user.
  RESULT_CODE_CLOUD_POLICY_ENROLLMENT_FAILED,

  // Chrome was downgraded since the last launch. Perform downgrade processing
  // and relaunch.
  RESULT_CODE_DOWNGRADE_AND_RELAUNCH,

  // Last return code (keep this last).
  RESULT_CODE_CHROME_LAST_CODE
};

static_assert(RESULT_CODE_CHROME_LAST_CODE == 34,
              "Please make sure the enum values are in sync with enums.xml");

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_RESULT_CODES_H_
