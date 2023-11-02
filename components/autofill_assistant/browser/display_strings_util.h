// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DISPLAY_STRINGS_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DISPLAY_STRINGS_UTIL_H_

#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// This function returns the string specified by display string ID. If the
// string is available in ClientSettings, we return that string, else a string
// from l10util is looked up for the corresponding Message in Chrome
// locale.
//
// This functionality is used by the backend to provide strings in a different
// locale compared to the one in the Chrome.
const std::string GetDisplayStringUTF8(
    ClientSettingsProto::DisplayStringId display_string_id,
    const ClientSettings& client_settings);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DISPLAY_STRINGS_UTIL_H_
