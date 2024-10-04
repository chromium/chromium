// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_MOCK_RENDERER_AGENT_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_MOCK_RENDERER_AGENT_H_

#include <memory>
#include <optional>

#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

// The `RendererAgent` with its dependencies on the `RenderFrame` mocked out.
//
// This approach is somewhat rudimentary, but appears to be the best compromise
// considering the alternatives:
//  -- Passing in a TestRenderFrame would itself require bringing up a
//     significant number of supporting classes.
//  -- Using a RenderViewTest would not allow having any non-filtered resource
//     loads due to not having a child thread and ResourceDispatcher.
// The real implementations of the mocked-out methods will be exercised in
// browsertests.
class MockRendererAgent : public RendererAgent {
 public:
  explicit MockRendererAgent(UnverifiedRulesetDealer* ruleset_dealer,
                             bool is_top_level_main_frame,
                             bool has_valid_opener);

  MockRendererAgent(const MockRendererAgent&) = delete;
  MockRendererAgent& operator=(const MockRendererAgent&) = delete;

  ~MockRendererAgent() override;

  MOCK_METHOD0(GetMainDocumentUrl, GURL());
  MOCK_METHOD0(GetInheritedActivationState,
               std::optional<subresource_filter::mojom::ActivationState>());
  MOCK_METHOD0(RequestActivationState, void());
  MOCK_METHOD0(OnSetFilterCalled, void());
  MOCK_METHOD0(OnSubresourceDisallowed, void());

  bool IsTopLevelMainFrame() override { return is_top_level_main_frame_; }

  bool HasValidOpener() override { return has_valid_opener_; }

  mojom::FingerprintingProtectionHost* GetFingerprintingProtectionHost()
      override {
    return nullptr;
  }

  void SetFilter(std::unique_ptr<subresource_filter::DocumentSubresourceFilter>
                     filter) override;

  subresource_filter::DocumentSubresourceFilter* filter() {
    return last_injected_filter_.get();
  }

  using RendererAgent::OnActivationComputed;

 private:
  const bool is_top_level_main_frame_;
  const bool has_valid_opener_;

  std::unique_ptr<subresource_filter::DocumentSubresourceFilter>
      last_injected_filter_;
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_MOCK_RENDERER_AGENT_H_
