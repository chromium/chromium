// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client_factory.h"

#include "components/fcm/engine/fcm_internals_builder.h"
#include "components/gcm_driver/gcm_client_impl.h"

namespace gcm {

std::unique_ptr<GCMClient> GCMClientFactory::BuildInstance() {
  return std::make_unique<GCMClientImpl>(
      std::make_unique<fcm::FcmInternalsBuilder>());
}

GCMClientFactory::GCMClientFactory() = default;

GCMClientFactory::~GCMClientFactory() = default;

}  // namespace gcm
