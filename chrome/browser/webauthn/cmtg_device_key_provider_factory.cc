// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/cmtg_device_key_provider_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/webauthn/core/browser/cmtg_device_key_provider.h"
#include "components/webauthn/core/browser/cryptauth_cmtg_device_key_provider.h"

CmtgDeviceKeyProviderFactory* CmtgDeviceKeyProviderFactory::GetInstance() {
  static base::NoDestructor<CmtgDeviceKeyProviderFactory> instance;
  return instance.get();
}

webauthn::CmtgDeviceKeyProvider* CmtgDeviceKeyProviderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<webauthn::CmtgDeviceKeyProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

CmtgDeviceKeyProviderFactory::CmtgDeviceKeyProviderFactory()
    : ProfileKeyedServiceFactory(
          "CmtgDeviceKeyProvider",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {}

CmtgDeviceKeyProviderFactory::~CmtgDeviceKeyProviderFactory() = default;

std::unique_ptr<KeyedService>
CmtgDeviceKeyProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<webauthn::CryptauthCmtgDeviceKeyProvider>();
}
