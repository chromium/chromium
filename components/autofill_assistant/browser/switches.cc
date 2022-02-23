// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/switches.h"

namespace autofill_assistant {
namespace switches {

// Enables annotating DOM when set to true.
const char kAutofillAssistantAnnotateDom[] =
    "autofill-assistant-enable-annotate-dom";

// Disables authentication when set to false. This is only useful
// during development, as prod instances require authentication.
const char kAutofillAssistantAuth[] = "autofill-assistant-auth";

// Sets the ECDSA public key to be used for autofill_assistant in base64.
const char kAutofillAssistantCupPublicKeyBase64[] =
    "autofill-assistant-cup-public-key-base64";

// Sets the key version of the ECDSA public key to be used for
// autofill_assistant.
const char kAutofillAssistantCupKeyVersion[] =
    "autofill-assistant-cup-key-version";

// Forces first-time user experience if set to 'true'. This will overwrite the
// AA preference by setting first time user to 'true' before each startup.
// Does nothing if unset or is set to false. This is only useful during testing
// and development.
// This flag is only for trigger scripts, because first-time user experience
// means that the user has not seen trigger script before.
const char kAutofillAssistantForceFirstTimeUser[] =
    "autofill-assistant-force-first-time-user";

// Forces the onboarding to be shown if set to 'true'. This will overwrite the
// AA preference by setting onboarding accepted to 'false' before each startup.
// Does nothing if unset or is set to false. This is only useful during testing
// and development.
const char kAutofillAssistantForceOnboarding[] =
    "autofill-assistant-force-onboarding";

// Base64-encoded |ImplicitTriggeringDebugParametersProto| containing debug
// parameters for in-CCT and in-Tab trigger scenarios.
const char kAutofillAssistantImplicitTriggeringDebugParameters[] =
    "autofill-assistant-implicit-triggering-debug-parameters";

// Sets the API key to be used instead of Chrome's default key when sending
// requests to the backend.
const char kAutofillAssistantServerKey[] = "autofill-assistant-key";

// Overrides the default backend URL.
const char kAutofillAssistantUrl[] = "autofill-assistant-url";

}  // namespace switches
}  // namespace autofill_assistant
