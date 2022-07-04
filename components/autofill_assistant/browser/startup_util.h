// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTUP_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTUP_UTIL_H_

#include "components/autofill_assistant/browser/trigger_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill_assistant {

enum class StartupMode {
  // Note: the entries in this list are not mutually exclusive. If more than one
  // are applicable, they take precedence in the order specified here, top-down.

  // A necessary feature was disabled. Depending on startup parameters, this
  // can refer to different features (check the log for more info).
  FEATURE_DISABLED,
  // A mandatory startup parameter was missing.
  MANDATORY_PARAMETERS_MISSING,
  // A required feature was turned off in the Chrome settings.
  SETTING_DISABLED,
  // No initial url was set, neither in ORIGINAL_DEEPLINK nor in the intent.
  NO_INITIAL_URL,

  // Parameters are ok, a regular script should be started immediately.
  START_REGULAR,
  // Parameters are ok, a remote trigger script should be started.
  START_RPC_TRIGGER_SCRIPT
};

// Provides startup utilities for autofill_assistant.
class StartupUtil {
 public:
  // Helper struct to facilitate instantiating this class.
  struct Options {
    // Whether the 'Make searches and browsing better' setting was enabled.
    bool msbb_setting_enabled = false;
    // Whether the 'Proactive help' setting was enabled.
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

  // Determines the startup URL. Preferably, the caller has passed this in
  // via ORIGINAL_DEEPLINK. If they have not, we try to guess the url from the
  // initial url. If this fails, this will return absl::nullopt.
  absl::optional<GURL> ChooseStartupUrlForIntent(
      const TriggerContext& trigger_context) const;
};

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTUP_UTIL_H_
