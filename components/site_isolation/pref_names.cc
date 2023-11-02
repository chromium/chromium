// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/pref_names.h"

namespace site_isolation {
namespace prefs {

// A list of origins that were heuristically determined to need process
// isolation due to an action triggered by the user. For example, an origin may
// be placed on this list in response to the user typing a password on it.
const char kUserTriggeredIsolatedOrigins[] =
    "site_isolation.user_triggered_isolated_origins";

// A list of origins that were determined to need process isolation based on
// heuristics triggered directly by web sites. For example, an origin may be
// placed on this list in response to serving Cross-Origin-Opener-Policy
// headers.  Unlike the user-triggered list above, web-triggered isolated
// origins are subject to stricter size and eviction policies, to guard against
// too many sites triggering isolation and to eventually stop isolation if web
// sites stop serving headers that triggered it.
const char kWebTriggeredIsolatedOrigins[] =
    "site_isolation.web_triggered_isolated_origins";

}  // namespace prefs
}  // namespace site_isolation
