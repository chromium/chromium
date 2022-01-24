// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_H_

namespace safe_browsing {

// Observes TailoredSecurityService bit.
class TailoredSecurityServiceObserver {
 public:
  // Called when the Tailored Security bit changed.
  virtual void OnTailoredSecurityBitChanged(bool enabled) = 0;

 protected:
  virtual ~TailoredSecurityServiceObserver() = default;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_H_
