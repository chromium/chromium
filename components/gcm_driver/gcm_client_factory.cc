// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client_factory.h"

#include "base/memory/ptr_util.h"
#include "components/gcm_driver/gcm_client_impl.h"

namespace gcm {

std::unique_ptr<GCMClient> GCMClientFactory::BuildInstance() {
  return std::unique_ptr<GCMClient>(new GCMClientImpl(
      base::WrapUnique<GCMInternalsBuilder>(new GCMInternalsBuilder())));
}

GCMClientFactory::GCMClientFactory() {
}

GCMClientFactory::~GCMClientFactory() {
}

}  // namespace gcm
