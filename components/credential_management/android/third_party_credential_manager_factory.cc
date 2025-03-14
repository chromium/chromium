// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/third_party_credential_manager_factory.h"

#include "components/credential_management/android/third_party_credential_manager_impl.h"

namespace credential_management {

ThirdPartyCredentialManagerFactory::ThirdPartyCredentialManagerFactory(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

std::unique_ptr<CredentialManagerInterface>
ThirdPartyCredentialManagerFactory::CreateCredentialManager() {
  return std::make_unique<
      credential_management::ThirdPartyCredentialManagerImpl>(
      render_frame_host_);
}
}  // namespace credential_management
