// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_FACTORY_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/credential_management/credential_manager_factory_interface.h"
#include "components/credential_management/credential_manager_interface.h"
#include "content/public/browser/render_frame_host.h"

namespace credential_management {
class ThirdPartyCredentialManagerFactory
    : public credential_management::CredentialManagerFactoryInterface {
 public:
  explicit ThirdPartyCredentialManagerFactory(
      content::RenderFrameHost* render_frame_host);
  std::unique_ptr<CredentialManagerInterface> CreateCredentialManager()
      override;

 public:
  raw_ptr<content::RenderFrameHost> render_frame_host_;
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_FACTORY_H_
