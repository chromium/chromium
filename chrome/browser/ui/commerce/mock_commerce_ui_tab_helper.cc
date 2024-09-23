// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace {
class TabFeaturesFake : public tabs::TabFeatures {
 public:
  TabFeaturesFake() = default;

 protected:
  std::unique_ptr<commerce::CommerceUiTabHelper> CreateCommerceUiTabHelper(
      content::WebContents* contents,
      Profile* profile) override {
    return std::make_unique<testing::NiceMock<MockCommerceUiTabHelper>>(
        contents, side_panel_registry());
  }
};
}  // namespace

// static
void MockCommerceUiTabHelper::ReplaceFactory() {
  tabs::TabFeatures::ReplaceTabFeaturesForTesting(
      base::BindRepeating([]() -> std::unique_ptr<tabs::TabFeatures> {
        return std::make_unique<TabFeaturesFake>();
      }));
}

MockCommerceUiTabHelper::MockCommerceUiTabHelper(content::WebContents* content,
                                                 SidePanelRegistry* registry)
    : CommerceUiTabHelper(content, nullptr, nullptr, nullptr, registry) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  valid_product_image_ = gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  // Set up a response so the default is success.
  ON_CALL(*this, SetPriceTrackingState)
      .WillByDefault([](bool enable, bool is_new_bookmark,
                        base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  ON_CALL(*this, CreateShoppingInsightsWebView).WillByDefault([]() {
    return std::make_unique<views::View>();
  });
}

MockCommerceUiTabHelper::~MockCommerceUiTabHelper() = default;

const gfx::Image& MockCommerceUiTabHelper::GetValidProductImage() {
  return valid_product_image_;
}

const gfx::Image& MockCommerceUiTabHelper::GetInvalidProductImage() {
  return empty_product_image_;
}
