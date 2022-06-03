// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_HELPER_H_

namespace security_interstitials {

// Returns true if the OS reports that the device is behind a captive portal.
bool IsBehindCaptivePortal();

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CAPTIVE_PORTAL_HELPER_H_
