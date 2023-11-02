// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/pref_names.h"

namespace embedder_support {

// A boolean pref set to true if we're using Link Doctor error pages.
const char kAlternateErrorPagesEnabled[] = "alternate_error_pages.enabled";

// Enum indicating if the user agent string should freeze the major version
// at 99 and report the browser's major version in the minor position.
// TODO(crbug.com/1290820): Remove this policy.
const char kForceMajorVersionToMinorPosition[] =
    "force_major_version_to_minor_position_in_user_agent";

// Enum indicating if the user agent reduction feature should be forced enabled
// or disabled. Defaults to blink::features::kReduceUserAgentMinorVersion trial.
const char kReduceUserAgentMinorVersion[] = "user_agent_reduction";

}  // namespace embedder_support
