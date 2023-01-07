// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rlz/rlz_tracker.h"

#include "rlz/lib/rlz_lib.h"
#include "ui/base/device_form_factor.h"

namespace rlz {

// static
rlz_lib::AccessPoint RLZTracker::ChromeOmnibox() {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE
             ? rlz_lib::CHROME_IOS_OMNIBOX_MOBILE
             : rlz_lib::CHROME_IOS_OMNIBOX_TABLET;
}

}  // namespace rlz
