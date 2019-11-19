// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"

#include "components/password_manager/core/browser/leak_detection/leak_detection_request.h"

namespace password_manager {

std::unique_ptr<LeakDetectionRequestInterface>
LeakDetectionRequestFactory::CreateNetworkRequest() const {
  return std::make_unique<LeakDetectionRequest>();
}

}  // namespace password_manager
