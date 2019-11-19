// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/safe_browsing_service_interface.h"

namespace safe_browsing {

SafeBrowsingServiceInterface*
SafeBrowsingServiceInterface::CreateSafeBrowsingService() {
  return factory_ ? factory_->CreateSafeBrowsingService() : nullptr;
}

// static
SafeBrowsingServiceFactory* SafeBrowsingServiceInterface::factory_ = nullptr;

}  // namespace safe_browsing
