// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"

#include "base/feature_list.h"
#include "base/memory/page_size.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
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
  if (base::FeatureList::IsEnabled(kSubresourceFilterPrewarm)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
                 "UnverifiedRulesetDealer Prewarm");
    cached_ruleset_ = GetRuleset();
    if (cached_ruleset_) {
      base::span<const uint8_t> data = cached_ruleset_->data();
      const size_t page_size = base::GetPageSize();
      for (size_t i = 0; i < data.size(); i += page_size) {
        const volatile uint8_t* ptr = &data[i];
        [[maybe_unused]] uint8_t dummy = *ptr;
      }
    }
  }
}

void UnverifiedRulesetDealer::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::SubresourceFilterRulesetObserver>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace subresource_filter
