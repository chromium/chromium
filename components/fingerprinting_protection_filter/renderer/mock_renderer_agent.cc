// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/mock_renderer_agent.h"

#include <utility>

#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

MockRendererAgent::MockRendererAgent(UnverifiedRulesetDealer* ruleset_dealer,
                                     bool is_top_level_main_frame,
                                     bool has_valid_opener)
    : RendererAgent(/*render_frame=*/nullptr, ruleset_dealer),
      is_top_level_main_frame_(is_top_level_main_frame),
      has_valid_opener_(has_valid_opener) {}

MockRendererAgent::~MockRendererAgent() = default;

void MockRendererAgent::SetFilter(
    std::unique_ptr<subresource_filter::DocumentSubresourceFilter> filter) {
  last_injected_filter_ = std::move(filter);
  OnSetFilterCalled();
}

}  // namespace fingerprinting_protection_filter
