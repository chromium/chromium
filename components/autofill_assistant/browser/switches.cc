// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/switches.h"

namespace autofill_assistant {
namespace switches {

// Sets the API key to be used instead of Chrome's default key when sending
// requests to the backend.
const char kAutofillAssistantServerKey[] = "autofill-assistant-key";

// Overrides the default backend URL.
const char kAutofillAssistantUrl[] = "autofill-assistant-url";

// Disables authentication when set to false. This is only useful
// during development, as prod instances require authentication.
const char kAutofillAssistantAuth[] = "autofill-assistant-auth";

// Forces the onboarding to be shown if set to 'true'. This will overwrite the
// AA preference by setting onboarding accepted to 'false' before each startup.
// Does nothing if unset or is set to false. This is only useful during testing
// and development.
const char kAutofillAssistantForceOnboarding[] =
    "autofill-assistant-force-onboarding";

}  // namespace switches
}  // namespace autofill_assistant
