// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"

#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "unverified_ruleset_dealer.h"

namespace fingerprinting_protection_filter {

UnverifiedRulesetDealer::UnverifiedRulesetDealer() = default;

UnverifiedRulesetDealer::~UnverifiedRulesetDealer() = default;

void UnverifiedRulesetDealer::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  // base::Unretained can be used here because the associated_interfaces
  // is owned by the RenderThread and will live for the duration of the
  // RenderThread.
  associated_interfaces->RemoveInterface(
      mojom::FingerprintingProtectionRulesetObserver::Name_);
  associated_interfaces
      ->AddInterface<mojom::FingerprintingProtectionRulesetObserver>(
          base::BindRepeating(
              &UnverifiedRulesetDealer::OnRendererAssociatedRequest,
              base::Unretained(this)));
}

void UnverifiedRulesetDealer::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      mojom::FingerprintingProtectionRulesetObserver::Name_);
}

void UnverifiedRulesetDealer::SetRulesetForProcess(base::File ruleset_file) {
  SetRulesetFile(std::move(ruleset_file));
}

void UnverifiedRulesetDealer::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<
        mojom::FingerprintingProtectionRulesetObserver> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace fingerprinting_protection_filter
