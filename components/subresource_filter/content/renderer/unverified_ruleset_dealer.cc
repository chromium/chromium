// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace subresource_filter {

UnverifiedRulesetDealer::UnverifiedRulesetDealer() = default;
UnverifiedRulesetDealer::~UnverifiedRulesetDealer() = default;

void UnverifiedRulesetDealer::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  // base::Unretained can be used here because the associated_interfaces
  // is owned by the RenderThread and will live for the duration of the
  // RenderThread.
  associated_interfaces->AddInterface<mojom::SubresourceFilterRulesetObserver>(
      base::BindRepeating(&UnverifiedRulesetDealer::OnRendererAssociatedRequest,
                          base::Unretained(this)));
}

void UnverifiedRulesetDealer::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      mojom::SubresourceFilterRulesetObserver::Name_);
}

void UnverifiedRulesetDealer::SetRulesetForProcess(base::File ruleset_file) {
  SetRulesetFile(std::move(ruleset_file));
}

void UnverifiedRulesetDealer::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::SubresourceFilterRulesetObserver>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace subresource_filter
