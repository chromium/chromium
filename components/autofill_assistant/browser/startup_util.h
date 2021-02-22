// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTUP_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTUP_UTIL_H_

#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// Provides startup utilities for autofill_assistant.
class StartupUtil {
 public:
  // Note: the entries in this list are not mutually exclusive. If more than one
  // are applicable, they take precedence in the order specified here, top-down.
  enum class StartupMode {
    // A necessary feature was disabled. Depending on startup parameters, this
    // can refer to different features (check the log for more info).
    FEATURE_DISABLED = 0,
    // A mandatory startup parameter was missing.
    MANDATORY_PARAMETERS_MISSING = 1,
    // A required feature was turned off in the Chrome settings.
    SETTING_DISABLED = 2,

    // Parameters are ok, a regular script should be started immediately.
    START_REGULAR = 3,
    // Parameters are ok, a base64 trigger script should be started.
    START_BASE64_TRIGGER_SCRIPT = 4,
    // Parameters are ok, a remote trigger script should be started.
    START_RPC_TRIGGER_SCRIPT = 5
  };

  // Helper struct to facilitate instantiating this class.
  struct Options {
    // Whether the 'Make searches and browsing better' setting was enabled.
    bool msbb_setting_enabled = false;
    // Whether the 'Proactive help" setting was enabled.
    bool proactive_help_setting_enabled = false;
    // Whether the feature module is already installed.
    bool feature_module_installed = false;
  };

  StartupUtil();
  ~StartupUtil();
  StartupUtil(const StartupUtil&) = delete;
  StartupUtil& operator=(const StartupUtil&) = delete;

  // Determines the correct startup mode based on |trigger_context| and
  // |options|.
  StartupMode ChooseStartupModeForIntent(const TriggerContext& trigger_context,
                                         const Options& options) const;
};

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTUP_UTIL_H_
