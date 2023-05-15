// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_RISK_UTIL_H_
#define CHROME_BROWSER_UI_AUTOFILL_RISK_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "ui/gfx/geometry/rect.h"

class PrefService;

namespace content {
class WebContents;
}

namespace autofill::risk_util {

// Loads risk data for the client, getting the device's risk fingerprint before
// calling |callback|. |obfuscated_gaia_id| is used in the fingerprinting
// process if provided. |web_contents| is used during fingerprinting as well,
// when retrieving user prefs, and in determining window bounds when not on
// Android. This function ends up calling the following overloaded
// LoadRiskData().
void LoadRiskData(uint64_t obfuscated_gaia_id,
                  content::WebContents* web_contents,
                  base::OnceCallback<void(const std::string&)> callback);

// LoadRiskDataHelper() retrieves all of the fields that do not use
// web contents, and then gets the device's fingerprint before calling
// |callback|. In situations where we do not have access to web contents, for
// example from the Clank settings page, we should call this implementation
// directly and let |web_contents| and |window_bounds| default to nullptr and
// empty, respectively. Callers with access to web contents should call the
// other version of this function above.
void LoadRiskDataHelper(uint64_t obfuscated_gaia_id,
                        PrefService* user_prefs,
                        base::OnceCallback<void(const std::string&)> callback,
                        content::WebContents* web_contents,
                        gfx::Rect window_bounds);

}  // namespace autofill::risk_util

#endif  // CHROME_BROWSER_UI_AUTOFILL_RISK_UTIL_H_
