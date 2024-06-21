// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_UNVERIFIED_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_UNVERIFIED_RULESET_DEALER_H_

#include <string>
#include <string_view>

#include "base/files/file.h"
#include "components/subresource_filter/core/common/ruleset_dealer.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace subresource_filter {

// Interface for a RulesetDealer that memory-maps a filtering ruleset file
// received over IPC from the RulesetDistributor and makes it available within
// the current render process through the GetRuleset() method. Does not make
// sure that the file is valid.
//
// Subclasses are responsible for implementing GetFilterTag() to decide
// whether to use a particular ruleset based whether its tag matches.
//
// See RulesetDealerBase for details on the lifetime of MemoryMappedRuleset, and
// the distribution pipeline diagram in content_ruleset_service.h.
class UnverifiedRulesetDealer : public RulesetDealer,
                                public content::RenderThreadObserver,
                                public mojom::SubresourceFilterRulesetObserver {
 public:
  UnverifiedRulesetDealer();

  UnverifiedRulesetDealer(const UnverifiedRulesetDealer&) = delete;
  UnverifiedRulesetDealer& operator=(const UnverifiedRulesetDealer&) = delete;

  ~UnverifiedRulesetDealer() override;

 private:
  // content::RenderThreadObserver overrides:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // Users of this interface should implement this function to choose the filter
  // they want the dealer to apply to.
  virtual std::string_view GetFilterTag() const = 0;

  // mojom::SubresourceFilterRulesetObserver overrides:
  void SetRulesetForProcess(
    const std::string& filter_tag, base::File ruleset_file) override;

  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::SubresourceFilterRulesetObserver>
          receiver);

  mojo::AssociatedReceiver<mojom::SubresourceFilterRulesetObserver> receiver_{
      this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_UNVERIFIED_RULESET_DEALER_H_
