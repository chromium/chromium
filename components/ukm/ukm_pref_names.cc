// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_pref_names.h"

namespace ukm {
namespace prefs {

// A random uint64 value unique for each chrome install.
const char kUkmClientId[] = "ukm.client_id";

// Preference which stores serialized UKM logs to be uploaded.
const char kUkmUnsentLogStore[] = "ukm.persisted_logs";

// Preference which stores the UKM session id.
const char kUkmSessionId[] = "ukm.session_id";

}  // namespace prefs
}  // namespace ukm
