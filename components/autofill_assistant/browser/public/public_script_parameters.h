// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PUBLIC_SCRIPT_PARAMETERS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PUBLIC_SCRIPT_PARAMETERS_H_

namespace autofill_assistant::public_script_parameters {

// Parameter that contains the current session username. Should be synced with
// |SESSION_USERNAME_PARAMETER| from
// .../password_manager/PasswordChangeLauncher.java
// TODO(b/151401974): Eliminate duplicate parameter definitions.
constexpr char kPasswordChangeUsernameParameterName[] =
    "PASSWORD_CHANGE_USERNAME";

// Whether the script should perform a login before changing the password.
constexpr char kPasswordChangeSkipLoginParameterName[] =
    "PASSWORD_CHANGE_SKIP_LOGIN";

// Special bool parameter that MUST be present in all intents. It allows the
// caller to either request immediate start of autofill assistant (if set to
// true), or a delayed start using trigger scripts (if set to false). If this is
// set to false, REQUEST_TRIGGER_SCRIPT or TRIGGER_SCRIPTS_BASE_64 must be set.
constexpr char kStartImmediatelyParameterName[] = "START_IMMEDIATELY";

// Mandatory parameter that MUST be present and set to true in all intents.
constexpr char kEnabledParameterName[] = "ENABLED";

// The original deeplink as indicated by the caller. Use this parameter instead
// of the initial URL when available to avoid issues where the initial URL
// points to a redirect rather than the actual deeplink.
constexpr char kOriginalDeeplinkParameterName[] = "ORIGINAL_DEEPLINK";

// The intent parameter.
constexpr char kIntentParameterName[] = "INTENT";

// Parameter name of the CALLER script parameter. Note that the corresponding
// values are integers, corresponding to the caller proto in the backend.
constexpr char kCallerParameterName[] = "CALLER";

// Parameter name of the SOURCE script parameter. Note that the corresponding
// values are integers, corresponding to the source proto in the backend.
constexpr char kSourceParameterName[] = "SOURCE";

// The name of the parameter that allows turning off RPC signing.
constexpr char kDisableRpcSigningParameterName[] = "DISABLE_RPC_SIGNING";

// Name of the debug script bundle. It has the following format:
// `{LDAP}/{BUNDLE_ID}/{INTENT_NAME}/{DOMAIN}`.
constexpr char kDebugBundleIdParameterName[] = "DEBUG_BUNDLE_ID";

// Name of the debug socket, e.g. for live debugging of the script run. It
// typically defaults to the debugging user's LDAP.
constexpr char kDebugSocketIdParameterName[] = "DEBUG_SOCKET_ID";

}  // namespace autofill_assistant::public_script_parameters

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PUBLIC_SCRIPT_PARAMETERS_H_
