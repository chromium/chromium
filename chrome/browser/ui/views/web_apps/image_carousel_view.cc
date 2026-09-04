// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/image_carousel_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace web_app {

namespace {

constexpr int kSpacingBetweenImages = 8;
constexpr int kThrobberDiameterValue = 50;
constexpr int kThrobberVerticalSpacing = 65;

// Custom layout that sets host_size to be same as the child view's size.
class ImageCarouselLayoutManager : public views::LayoutManagerBase {
 public:
  ImageCarouselLayoutManager() = default;
  ~ImageCarouselLayoutManager() override = default;

 protected:
  // LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout layout;
    views::View* const inner_container = host_view()->children().front();

    const gfx::Size item_size(inner_container->GetPreferredSize());
    layout.child_layouts.push_back({inner_container, true,
                                    gfx::Rect(gfx::Point(0, 0), item_size),
                                    views::SizeBounds(item_size)});

    layout.host_size = item_size;
    return layout;
  }
};

class ScrollButton : public views::ImageButton {
  METADATA_HEADER(ScrollButton, views::ImageButton)

 public:
  ScrollButton(ImageCarouselView::ButtonType button_type,
               PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    ConfigureVectorImageButton(this);

    SetBackground(views::CreateRoundedRectBackground(ui::kColorButtonBackground,
                                                     web_app::kIconSize / 2));

    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));

    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        button_type == ImageCarouselView::ButtonType::kLeading
            ? IDS_ACCNAME_WEB_APP_DETAILED_INSTALL_DIALOG_LEADING_SCROLL_BUTTON
            : IDS_ACCNAME_WEB_APP_DETAILED_INSTALL_DIALOG_TRAILING_SCROLL_BUTTON));

    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        button_type == ImageCarouselView::ButtonType::kLeading
            ? ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                                 ? kKeyboardArrowLeftIcon
                                                 : kLeadingScrollOldIcon,
                                             ui::kColorIcon)
            : ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                                 ? kKeyboardArrowRightIcon
                                                 : kTrailingScrollOldIcon,
                                             ui::kColorIcon));

    views::InkDrop::Get(this)->SetBaseColor(
        views::TypographyProvider::Get().GetColorId(
            views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY));

    ink_drop_container_ =
        AddChildView(std::make_unique<views::InkDropContainerView>());
  }
  ScrollButton(const ScrollButton&) = delete;
  ScrollButton& operator=(const ScrollButton&) = delete;
  ~ScrollButton() override = default;

  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override {
    ink_drop_container_->AddLayerToRegion(layer, region);
  }

  void RemoveLayerFromRegions(ui::Layer* layer) override {
    ink_drop_container_->RemoveLayerFromRegions(layer);
  }

 private:
  raw_ptr<views::InkDropContainerView> ink_drop_container_ = nullptr;
};

BEGIN_METADATA(ScrollButton)
END_METADATA

}  // namespace

ImageCarouselView::ImageCarouselView(
    base::WeakPtr<WebAppScreenshotFetcher> fetcher,
    gfx::Insets inner_container_insets)
    : fetcher_(fetcher) {
  SetUseDefaultFillLayout(true);

  image_padding_ = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  image_container_ = AddChildView(std::make_unique<views::View>());

  image_inner_container_ =
      image_container_->AddChildView(std::make_unique<views::BoxLayoutView>());
  image_inner_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  if (!inner_container_insets.IsEmpty()) {
    image_inner_container_->SetInsideBorderInsets(inner_container_insets);
  }
  image_inner_container_->SetProperty(views::kElementIdentifierKey,
                                      kDetailedInstallDialogImageContainer);

  for (const gfx::Size& screenshot_size : fetcher_->GetScreenshotSizes()) {
    auto throbber_container_view = std::make_unique<views::BoxLayoutView>();
    const int throbber_horizontal_inset = base::checked_cast<int>(
        (GetScaledWidthBasedOnThrobberHeight(screenshot_size) -
         kThrobberDiameterValue) /
        2);

    auto throbber = std::make_unique<views::Throbber>(kThrobberDiameterValue);
    throbber->SetColorId(ui::kColorSysTertiaryContainer);
    throbber->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(kThrobberVerticalSpacing, throbber_horizontal_inset));
    throbber->Start();

    throbber_container_view->AddChildView(std::move(throbber));
    throbber_container_view->SetBorder(views::CreateSolidBorder(
        /*thickness=*/1, ui::kColorSysSecondaryContainer));
    throbber_container_view->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, image_padding_));
    image_inner_container_->AddChildView(std::move(throbber_container_view));
  }

  image_container_->SetLayoutManager(
      std::make_unique<ImageCarouselLayoutManager>());

  bounds_animator_ =
      std::make_unique<views::BoundsAnimator>(image_container_, false);
  bounds_animator_->SetAnimationDuration(base::Seconds(0.5));

  auto leading_button_container = std::make_unique<views::BoxLayoutView>();
  leading_button_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  leading_button_ =
      leading_button_container->AddChildView(std::make_unique<ScrollButton>(
          ButtonType::kLeading,
          base::BindRepeating(&ImageCarouselView::OnScrollButtonClicked,
                              weak_ptr_factory_.GetWeakPtr(),
                              ButtonType::kLeading)));
  leading_button_container_ = AddChildView(std::move(leading_button_container));
  leading_button_->SetVisible(false);

  auto trailing_button_container = std::make_unique<views::BoxLayoutView>();
  trailing_button_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  trailing_button_ =
      trailing_button_container->AddChildView(std::make_unique<ScrollButton>(
          ButtonType::kTrailing,
          base::BindRepeating(&ImageCarouselView::OnScrollButtonClicked,
                              weak_ptr_factory_.GetWeakPtr(),
                              ButtonType::kTrailing)));
  trailing_button_container_ =
      AddChildView(std::move(trailing_button_container));
}

ImageCarouselView::~ImageCarouselView() = default;

void ImageCarouselView::AddedToWidget() {
  for (size_t i = 0; i < fetcher_->GetScreenshotSizes().size(); i++) {
    fetcher_->GetScreenshot(
        i, base::BindOnce(&ImageCarouselView::OnScreenshotFetched,
                          weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void ImageCarouselView::OnScreenshotFetched(
    int index,
    SkBitmap bitmap,
    std::optional<std::u16string> label) {
  CHECK(index < static_cast<int>(image_inner_container_->children().size()));
  if (bitmap.drawsNothing()) {
    return;
  }

  float current_scale =
      display::Screen::Get()
          ->GetPreferredScaleFactorForView(GetWidget()->GetNativeView())
          .value_or(1.0f);

  auto image_view = std::make_unique<views::ImageView>();
  ui::ImageModel screenshot = ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFromBitmap(bitmap, current_scale));
  image_view->SetImage(screenshot);
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets::TLBR(0, 0, 0, image_padding_));

  const gfx::Size current_image_size(screenshot.GetImage().Width(),
                                     screenshot.GetImage().Height());
  image_view->SetImageSize(
      {GetScaledWidthBasedOnThrobberHeight(current_image_size),
       GetFullThrobberHeight()});
  if (label) {
    image_view->GetViewAccessibility().SetName(label.value());
  }

  image_inner_container_->RemoveChildViewT(
      image_inner_container_->children()[index]);
  image_inner_container_->AddChildViewAt(std::move(image_view), index);

  InvalidateLayout();
}

void ImageCarouselView::Layout(PassKey) {
  image_container_->SetBounds(0, 0, width(), height());

  if (!trailing_button_visibility_set_up_) {
    image_carousel_full_width_ =
        image_inner_container_->GetPreferredSize().width();
    trailing_button_->SetVisible(image_carousel_full_width_ > width());
    trailing_button_visibility_set_up_ = true;
  }

  leading_button_container_->SetBounds(
      kSpacingBetweenImages, 0, web_app::kIconSize, GetFullThrobberHeight());

  trailing_button_container_->SetBounds(
      width() - kSpacingBetweenImages - web_app::kIconSize, 0,
      web_app::kIconSize, GetFullThrobberHeight());
}

void ImageCarouselView::OnScrollButtonClicked(ButtonType button_type) {
  DCHECK(image_inner_container_->children().size());

  int image_width =
      image_inner_container_->children().front()->bounds().width() +
      image_padding_;
  int container_width = image_container_->bounds().width();

  int delta = image_width * (container_width / image_width);

  if (button_type == ButtonType::kTrailing) {
    delta = -delta;
  }

  const gfx::Rect& bounds = image_inner_container_->bounds();
  int x = bounds.x() + delta;

  x = std::min(x, 0);
  x = std::max(x, (container_width - image_carousel_full_width_));

  leading_button_->SetVisible(x < 0);

  trailing_button_->SetVisible(x + image_carousel_full_width_ >
                               container_width);

  bounds_animator_->AnimateViewTo(
      image_inner_container_,
      gfx::Rect(x, bounds.y(), bounds.width(), bounds.height()));
}

int ImageCarouselView::GetScaledWidthBasedOnThrobberHeight(
    const gfx::Size& size) {
  const int throbber_height = GetFullThrobberHeight();
  CHECK_GT(size.height(), 0) << "screenshot cannot have an empty height";
  int height_limited_width = base::checked_cast<int>(
      size.width() *
      (base::checked_cast<float>(throbber_height) / size.height()));
  int clamped_width_per_screenshot_ratio = base::checked_cast<int>(
      throbber_height * webapps::kMaximumScreenshotRatio);
  return std::min(height_limited_width, clamped_width_per_screenshot_ratio);
}

int ImageCarouselView::GetFullThrobberHeight() {
  return 2 * kThrobberVerticalSpacing + kThrobberDiameterValue;
}

BEGIN_METADATA(ImageCarouselView)
END_METADATA

}  // namespace web_app
