// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chromeos/passkey_discovery.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/chromeos/passkey_service_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_transport_protocol.h"

namespace chromeos {

PasskeyDiscovery::PasskeyDiscovery(content::RenderFrameHost* rfh)
    : FidoDiscoveryBase(device::FidoTransportProtocol::kInternal),
      render_frame_host_id_(rfh->GetGlobalId()) {}

PasskeyDiscovery::~PasskeyDiscovery() = default;

void PasskeyDiscovery::Start() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PasskeyDiscovery::StartDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void PasskeyDiscovery::StartDiscovery() {
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(rfh);
  auto* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  auto authenticator = std::make_unique<PasskeyAuthenticator>(
      rfh, PasskeyServiceFactory::GetForProfile(profile),
      PasskeyModelFactory::GetForProfile(profile));
  auto* ptr = authenticator.get();
  authenticators_.emplace_back(std::move(authenticator));
  observer()->DiscoveryStarted(this, /*success=*/true, {ptr});
}

}  // namespace chromeos
