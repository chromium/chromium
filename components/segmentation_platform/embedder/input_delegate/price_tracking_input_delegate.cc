// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/price_tracking_input_delegate.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"

namespace segmentation_platform::processing {

PriceTrackingInputDelegate::PriceTrackingInputDelegate(
    ShoppingServiceGetter shopping_service_getter,
    BookmarkModelGetter bookmark_model_getter)
    : shopping_service_getter_(std::move(shopping_service_getter)),
      bookmark_model_getter_(std::move(bookmark_model_getter)),
      weak_ptr_factory_(this) {}

PriceTrackingInputDelegate::~PriceTrackingInputDelegate() = default;

void PriceTrackingInputDelegate::Process(
    const proto::CustomInput& input,
    const FeatureProcessorState& feature_processor_state,
    ProcessedCallback callback) {
  const auto& input_context = feature_processor_state.input_context();
  if (!input_context) {
    std::move(callback).Run(/*error=*/true, Tensor());
    return;
  }

  auto iter = input_context->metadata_args.find("url");
  if (iter == input_context->metadata_args.end()) {
    std::move(callback).Run(/*error=*/true, Tensor());
    return;
  }

  if (iter->second.type != processing::ProcessedValue::Type::URL) {
    std::move(callback).Run(/*error=*/true, Tensor());
    return;
  }

  auto url = *iter->second.url.get();
  commerce::ShoppingService* shopping_service = shopping_service_getter_.Run();
  if (!shopping_service) {
    std::move(callback).Run(/*error=*/true, Tensor());
    return;
  }

  bookmarks::BookmarkModel* bookmark_model = bookmark_model_getter_.Run();
  if (!bookmark_model) {
    std::move(callback).Run(/*error=*/true, Tensor());
    return;
  }

  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url);

  if (bookmark_node) {
    bool is_price_tracked =
        commerce::IsBookmarkPriceTracked(bookmark_model, bookmark_node);

    if (is_price_tracked) {
      std::vector<ProcessedValue> output;
      output.emplace_back(0.0f);
      std::move(callback).Run(/*error=*/false, std::move(output));
      return;
    }
  }

  shopping_service->GetProductInfoForUrl(
      url, base::BindOnce(&PriceTrackingInputDelegate::OnProductInfoReceived,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PriceTrackingInputDelegate::OnProductInfoReceived(
    ProcessedCallback callback,
    const GURL& url,
    const absl::optional<commerce::ProductInfo>& product_info) {
  std::vector<ProcessedValue> output;
  if (product_info) {
    output.emplace_back(1.0f);
  } else {
    output.emplace_back(0.0f);
  }
  std::move(callback).Run(/*error=*/false, std::move(output));
}

}  // namespace segmentation_platform::processing
