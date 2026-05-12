// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

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

enum class ButtonType { kLeading, kTrailing };
class ScrollButton : public views::ImageButton {
  METADATA_HEADER(ScrollButton, views::ImageButton)

 public:
  ScrollButton(ButtonType button_type, PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    ConfigureVectorImageButton(this);

    SetBackground(views::CreateRoundedRectBackground(ui::kColorButtonBackground,
                                                     web_app::kIconSize / 2));

    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));

    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        button_type == ButtonType::kLeading
            ? IDS_ACCNAME_WEB_APP_DETAILED_INSTALL_DIALOG_LEADING_SCROLL_BUTTON
            : IDS_ACCNAME_WEB_APP_DETAILED_INSTALL_DIALOG_TRAILING_SCROLL_BUTTON));

    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        button_type == ButtonType::kLeading
            ? ui::ImageModel::FromVectorIcon(kLeadingScrollIcon, ui::kColorIcon)
            : ui::ImageModel::FromVectorIcon(kTrailingScrollIcon,
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

class ImageCarouselView : public views::View {
  METADATA_HEADER(ImageCarouselView, views::View)

 public:
  explicit ImageCarouselView(
      base::WeakPtr<web_app::WebAppScreenshotFetcher> fetcher)
      : fetcher_(fetcher) {
    SetUseDefaultFillLayout(true);

    image_padding_ = views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
    image_container_ = AddChildView(std::make_unique<views::View>());

    image_inner_container_ = image_container_->AddChildView(
        std::make_unique<views::BoxLayoutView>());
    image_inner_container_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    image_inner_container_->SetProperty(
        views::kElementIdentifierKey,
        web_app::kDetailedInstallDialogImageContainer);

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
    leading_button_container_ =
        AddChildView(std::move(leading_button_container));
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

  void AddedToWidget() override {
    for (size_t i = 0; i < fetcher_->GetScreenshotSizes().size(); i++) {
      fetcher_->GetScreenshot(
          i, base::BindOnce(&ImageCarouselView::OnScreenshotFetched,
                            weak_ptr_factory_.GetWeakPtr(), i));
    }
  }

  void OnScreenshotFetched(int index,
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

    image_view->SetImageSize(
        {GetScaledWidthBasedOnThrobberHeight(screenshot.GetImage().Size()),
         GetFullThrobberHeight()});
    if (label) {
      image_view->GetViewAccessibility().SetName(label.value());
    }

    image_inner_container_->RemoveChildViewT(
        image_inner_container_->children()[index]);
    image_inner_container_->AddChildViewAt(std::move(image_view), index);

    InvalidateLayout();
  }

  void Layout(PassKey) override {
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

 private:
  void OnScrollButtonClicked(ButtonType button_type) {
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

  int GetScaledWidthBasedOnThrobberHeight(const gfx::Size& size) {
    const int throbber_height = GetFullThrobberHeight();
    CHECK_GT(size.height(), 0) << "screenshot cannot have an empty height";
    int height_limited_width = base::checked_cast<int>(
        size.width() *
        (base::checked_cast<float>(throbber_height) / size.height()));
    int clamped_width_per_screenshot_ratio = base::checked_cast<int>(
        throbber_height * webapps::kMaximumScreenshotRatio);
    return std::min(height_limited_width, clamped_width_per_screenshot_ratio);
  }

  int GetFullThrobberHeight() {
    return 2 * kThrobberVerticalSpacing + kThrobberDiameterValue;
  }

  base::WeakPtr<web_app::WebAppScreenshotFetcher> fetcher_;
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

BEGIN_METADATA(ImageCarouselView)
END_METADATA

}  // namespace

// static
std::unique_ptr<WebAppInstallIntroView> WebAppInstallIntroView::Create(
    InstallDialogType install_type,
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    const std::u16string& description,
    base::WeakPtr<WebAppScreenshotFetcher> fetcher,

    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  return base::WrapUnique(new WebAppInstallIntroView(
      install_type, icon_image, app_name, start_url, is_maskable, description,
      fetcher, std::move(text_tracker_callback)));
}

WebAppInstallIntroView::WebAppInstallIntroView(
    InstallDialogType install_type,
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    const std::u16string& description,
    base::WeakPtr<WebAppScreenshotFetcher> fetcher,
    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  int vertical_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));

  switch (install_type) {
    case InstallDialogType::kDiy: {
      auto site_icon_view = std::make_unique<SiteIconTextAndOriginView>(
          icon_image, app_name,
          l10n_util::GetStringUTF16(IDS_DIY_APP_AX_BUBBLE_NAME_LABEL),
          start_url, nullptr, std::move(text_tracker_callback));
      textfield_ = site_icon_view->title_field();
      AddChildView(std::move(site_icon_view));
      break;
    }
    case InstallDialogType::kDetailed: {
      CHECK(fetcher);
      AddChildView(WebAppIconNameAndOriginView::Create(icon_image, app_name,
                                                       start_url, is_maskable));
      auto description_label = std::make_unique<views::Label>(description);
      description_label->SetMultiLine(true);
      description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      description_label->SetTextStyle(views::style::STYLE_SECONDARY);
      AddChildView(std::move(description_label));
      AddChildView(std::make_unique<ImageCarouselView>(fetcher));
      break;
    }
    case InstallDialogType::kSimple:
      AddChildView(WebAppIconNameAndOriginView::Create(icon_image, app_name,
                                                       start_url, is_maskable));
      break;
  }
}

WebAppInstallIntroView::~WebAppInstallIntroView() = default;

BEGIN_METADATA(WebAppInstallIntroView)
END_METADATA

}  // namespace web_app
