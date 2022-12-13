// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_PRICE_TRACKING_INPUT_DELEGATE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_PRICE_TRACKING_INPUT_DELEGATE_H_

#include "components/segmentation_platform/public/input_delegate.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace commerce {
struct ProductInfo;
class ShoppingService;
}  // namespace commerce

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace segmentation_platform::processing {

// InputDelegate implementation that handles FillPolicy::PRICE_TRACKING_HINTS.
// The lifetime of this class is tied to SegmentationPlatformService.
class PriceTrackingInputDelegate : public InputDelegate {
 public:
  // A repeating callback used to fetch ShoppingService.
  using ShoppingServiceGetter =
      base::RepeatingCallback<commerce::ShoppingService*()>;
  using BookmarkModelGetter =
      base::RepeatingCallback<bookmarks::BookmarkModel*()>;

  explicit PriceTrackingInputDelegate(
      ShoppingServiceGetter shopping_service_getter,
      BookmarkModelGetter bookmark_model_getter);
  ~PriceTrackingInputDelegate() override;

  PriceTrackingInputDelegate(const PriceTrackingInputDelegate&) = delete;
  PriceTrackingInputDelegate& operator=(const PriceTrackingInputDelegate&) =
      delete;

  // InputDelegate overrides.
  void Process(const proto::CustomInput& input,
               const FeatureProcessorState& feature_processor_state,
               ProcessedCallback callback) override;

 private:
  // Callback invoked with product info response.
  void OnProductInfoReceived(
      ProcessedCallback callback,
      const GURL& url,
      const absl::optional<commerce::ProductInfo>& product_info);

  // Callback to fetch shopping service. Shouldn't be invoked after the platform
  // is destroyed.
  ShoppingServiceGetter shopping_service_getter_;
  // Callback to fetch bookmark model.
  BookmarkModelGetter bookmark_model_getter_;

  base::WeakPtrFactory<PriceTrackingInputDelegate> weak_ptr_factory_;
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_PRICE_TRACKING_INPUT_DELEGATE_H_
