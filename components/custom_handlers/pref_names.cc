// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/pref_names.h"

namespace custom_handlers {
namespace prefs {

// List of protocol handlers.
const char kRegisteredProtocolHandlers[] =
    "custom_handlers.registered_protocol_handlers";

// List of protocol handlers the user has requested not to be asked about again.
const char kIgnoredProtocolHandlers[] =
    "custom_handlers.ignored_protocol_handlers";

// List of protocol handlers registered by policy.
const char kPolicyRegisteredProtocolHandlers[] =
    "custom_handlers.policy.registered_protocol_handlers";

// List of protocol handlers the policy has requested to be ignored.
const char kPolicyIgnoredProtocolHandlers[] =
    "custom_handlers.policy.ignored_protocol_handlers";

// Whether user-specified handlers for protocols and content types can be
// specified.
const char kCustomHandlersEnabled[] = "custom_handlers.enabled";

}  // namespace prefs
}  // namespace custom_handlers
