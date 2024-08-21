// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_UNVERIFIED_RULESET_DEALER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_UNVERIFIED_RULESET_DEALER_H_

#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/core/common/ruleset_dealer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace subresource_filter {
class MemoryMappedRuleset;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

// Memory maps the Fingerprinting Protection filtering ruleset file received
// over IPC from the `RulesetDistributor`, and makes it available to all
// `FingerprintingProtectionRendererAgents` within the current render process
// through the `GetRuleset()` method. Does not make sure that the file is valid.
//
// See `subresource_filter::RulesetDealerBase` for details on the lifetime of
// `subresource_filter::MemoryMappedRuleset`, and the distribution pipeline
// diagram in the `subresource_filter::RulesetService` header file.
class UnverifiedRulesetDealer
    : public subresource_filter::RulesetDealer,
      public content::RenderThreadObserver,
      public mojom::FingerprintingProtectionRulesetObserver {
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

  // mojom::FingerprintingProtectionRulesetObserver overrides:
  void SetRulesetForProcess(base::File ruleset_file) override;

  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<
          mojom::FingerprintingProtectionRulesetObserver> receiver);

  mojo::AssociatedReceiver<mojom::FingerprintingProtectionRulesetObserver>
      receiver_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_UNVERIFIED_RULESET_DEALER_H_
