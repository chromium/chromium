// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_session_request_consent_manager.h"

namespace vr {
namespace {
XRSessionRequestConsentManager* g_consent_manager = nullptr;
XRSessionRequestConsentManager* g_consent_manager_for_testing = nullptr;
}  // namespace

XRSessionRequestConsentManager* XRSessionRequestConsentManager::Instance() {
  DCHECK(g_consent_manager || g_consent_manager_for_testing);
  if (g_consent_manager_for_testing)
    return g_consent_manager_for_testing;
  return g_consent_manager;
}

void XRSessionRequestConsentManager::SetInstance(
    XRSessionRequestConsentManager* instance) {
  DCHECK(instance);
  DCHECK(!g_consent_manager);
  g_consent_manager = instance;
}

void XRSessionRequestConsentManager::SetInstanceForTesting(
    XRSessionRequestConsentManager* instance) {
  g_consent_manager_for_testing = instance;
}

XRSessionRequestConsentManager::XRSessionRequestConsentManager() = default;

XRSessionRequestConsentManager::~XRSessionRequestConsentManager() = default;

}  // namespace vr
