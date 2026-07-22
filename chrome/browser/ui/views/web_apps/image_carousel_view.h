// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_IMAGE_CAROUSEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_IMAGE_CAROUSEL_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"

namespace views {
class BoundsAnimator;
class BoxLayoutView;
}  // namespace views

namespace web_app {

class WebAppScreenshotFetcher;

// A view that displays screenshot images in a horizontal scrollable carousel
// with throbbers while loading and scroll buttons for navigation.
class ImageCarouselView : public views::View {
  METADATA_HEADER(ImageCarouselView, views::View)

 public:
  enum class ButtonType { kLeading, kTrailing };

  explicit ImageCarouselView(
      base::WeakPtr<WebAppScreenshotFetcher> fetcher,
      gfx::Insets inner_container_insets = gfx::Insets());
  ImageCarouselView(const ImageCarouselView&) = delete;
  ImageCarouselView& operator=(const ImageCarouselView&) = delete;
  ~ImageCarouselView() override;

  // views::View:
  void AddedToWidget() override;
  void Layout(PassKey) override;

 private:
  void OnScreenshotFetched(int index,
                           SkBitmap bitmap,
                           std::optional<std::u16string> label);
  void OnScrollButtonClicked(ButtonType button_type);

  int GetScaledWidthBasedOnThrobberHeight(const gfx::Size& size);
  int GetFullThrobberHeight();

  base::WeakPtr<WebAppScreenshotFetcher> fetcher_;
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;
  raw_ptr<views::View> image_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> image_inner_container_ = nullptr;
  raw_ptr<views::View> leading_button_ = nullptr;
  raw_ptr<views::View> trailing_button_ = nullptr;
  raw_ptr<views::View> leading_button_container_ = nullptr;
  raw_ptr<views::View> trailing_button_container_ = nullptr;
  int image_carousel_full_width_ = 0;
  int image_padding_ = 0;
  bool trailing_button_visibility_set_up_ = false;
  base::WeakPtrFactory<ImageCarouselView> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_IMAGE_CAROUSEL_VIEW_H_
