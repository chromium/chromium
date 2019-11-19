// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_provider_base.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_provider_base_factory.h"

namespace web_app {

// static
WebAppProviderBase* WebAppProviderBase::GetProviderBase(Profile* profile) {
  return WebAppProviderBaseFactory::GetForProfile(profile);
}

WebAppProviderBase::WebAppProviderBase() = default;

WebAppProviderBase::~WebAppProviderBase() = default;

}  // namespace web_app
