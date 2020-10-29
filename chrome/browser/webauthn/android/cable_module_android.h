// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_

namespace webauthn {
namespace authenticator {

// RegisterForCloudMessages installs a |GCMAppHandler| that handles caBLEv2
// message in the |GCMDriver| connected to the primary profile. This should be
// called during browser startup to ensure that the |GCMAppHandler| is
// registered before any GCM messages are processed. (Otherwise they will be
// dropped.)
void RegisterForCloudMessages();

}  // namespace authenticator
}  // namespace webauthn

#endif
