// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/quirks/pref_names.h"

namespace quirks {
namespace prefs {

// A dictionary of info for Quirks Client/Server interaction, mostly last server
// request times, keyed to display product_id's.
const char kQuirksClientLastServerCheck[] = "quirks_client.last_server_check";

}  // namespace prefs
}  // namespace quirks
