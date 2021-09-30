// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/zero_trust_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace enterprise_connectors {

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    DeviceTrustService* device_trust_service,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      device_trust_service_(device_trust_service),
      profile_(profile) {
  DCHECK(device_trust_service_);
  DCHECK(profile_);
}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetZeroTrustState(
    GetZeroTrustStateCallback callback) {
  auto state = connectors_internals::mojom::ZeroTrustState::New(
      device_trust_service_->IsEnabled(),
      utils::SignalsToMap(device_trust_service_->GetSignals()));
  std::move(callback).Run(std::move(state));
}

}  // namespace enterprise_connectors
