// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace {

constexpr int kSpacingBetweenImages = 8;

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

enum class ButtonType { LEADING, TRAILING };
class ScrollButton : public views::ImageButton {
  METADATA_HEADER(ScrollButton, views::ImageButton)

 public:
  ScrollButton(ButtonType button_type, PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    ConfigureVectorImageButton(this);

    SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorButtonBackground, web_app::kIconSize / 2));

    views::HighlightPathGenerator::Install(
        this,
        std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets()));

    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        button_type == ButtonType::LEADING
            ? IDS_ACCNAME_WEB_APP_DETAILED_INSTALL_DIALOG_LEADING_SCROLL_BUTTON
            : IDS_ACCNAME_WEB_APP_DETAILED_INSTALL_DIALOG_TRAILING_SCROLL_BUTTON));

    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        button_type == ButtonType::LEADING
            ? ui::ImageModel::FromVectorIcon(kLeadingScrollIcon, ui::kColorIcon)
            : ui::ImageModel::FromVectorIcon(kTrailingScrollIcon,
                                             ui::kColorIcon));

    views::InkDrop::Get(this)->SetBaseColorId(
        views::TypographyProvider::Get().GetColorId(
            views::style::CONTEXT_BUTTON, views::style::STYLE_SECONDARY));

    ink_drop_container_ =
        AddChildView(std::make_unique<views::InkDropContainerView>());
  }
  ScrollButton(const ScrollButton&) = delete;
  ScrollButton& operator=(const ScrollButton&) = delete;
  ~ScrollButton() override = default;

  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override {
    // This routes background layers to `ink_drop_container_` instead of `this`
    // to avoid painting effects underneath our background.
    ink_drop_container_->AddLayerToRegion(layer, region);
  }

  void RemoveLayerFromRegions(ui::Layer* layer) override {
    // This routes background layers to `ink_drop_container_` instead of `this`
    // to avoid painting effects underneath our background.
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
  explicit ImageCarouselView(std::vector<webapps::Screenshot> screenshots)
      : screenshots_(std::move(screenshots)) {
    DCHECK(screenshots_.size());

    // Use a fill layout to draw the buttons container on
    // top of the image carousel.
    SetUseDefaultFillLayout(true);

    // Screenshots are sanitized by `InstallableManager::OnScreenshotFetched`
    // and should all have the same aspect ratio.
#if DCHECK_IS_ON()
    for (const auto& screenshot : screenshots_) {
      DCHECK(screenshot.image.width() * screenshots_[0].image.height() ==
             screenshot.image.height() * screenshots_[0].image.width());
    }
#endif

    image_padding_ = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
    image_container_ = AddChildView(std::make_unique<views::View>());

    image_inner_container_ = image_container_->AddChildView(
        std::make_unique<views::BoxLayoutView>());
    image_inner_container_->SetBetweenChildSpacing(image_padding_);

    for (size_t i = 0; i < screenshots_.size(); i++) {
      image_views_.push_back(image_inner_container_->AddChildView(
          std::make_unique<views::ImageView>()));
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
            ButtonType::LEADING,
            base::BindRepeating(&ImageCarouselView::OnScrollButtonClicked,
                                base::Unretained(this), ButtonType::LEADING)));
    leading_button_container_ =
        AddChildView(std::move(leading_button_container));
    leading_button_->SetVisible(false);

    auto trailing_button_container = std::make_unique<views::BoxLayoutView>();
    trailing_button_container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    trailing_button_ =
        trailing_button_container->AddChildView(std::make_unique<ScrollButton>(
            ButtonType::TRAILING,
            base::BindRepeating(&ImageCarouselView::OnScrollButtonClicked,
                                base::Unretained(this), ButtonType::TRAILING)));
    trailing_button_container_ =
        AddChildView(std::move(trailing_button_container));
  }

  void AddedToWidget() override {
    float current_scale =
        display::Screen::GetScreen()
            ->GetPreferredScaleFactorForView(GetWidget()->GetNativeView())
            .value_or(1.0f);
    for (size_t i = 0; i < screenshots_.size(); i++) {
      image_views_[i]->SetImage(
          ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFromBitmap(
              screenshots_[i].image, current_scale)));
      if (screenshots_[i].label) {
        image_views_[i]->GetViewAccessibility().SetName(
            screenshots_[i].label.value());
      }
    }
  }

  void Layout(PassKey) override {
    // Use a fixed height that guarantees to fit the screenshot with max ratio
    // and still show a clip for the next screenshot.
    const int fixed_height = base::checked_cast<int>(
        base::checked_cast<float>(width() - image_padding_ * 2) /
        webapps::kMaximumScreenshotRatio);
    image_container_->SetBounds(0, 0, width(), height());

    // Only setup the initial visibility and screenshots size once based on
    // container width & max screenshot ratio, the visibility is later updated
    // by `OnScrollButtonClicked` based on image carousel animation.
    if (!trailing_button_visibility_set_up_) {
      for (size_t i = 0; i < screenshots_.size(); i++) {
        const int item_width =
            base::checked_cast<int>(screenshots_[i].image.width() *
                                    (base::checked_cast<float>(fixed_height) /
                                     screenshots_[i].image.height()));
        image_views_[i]->SetImageSize({item_width, fixed_height});
      }
      image_carousel_full_width_ =
          image_inner_container_->GetPreferredSize().width();
      trailing_button_->SetVisible(image_carousel_full_width_ > width());
      trailing_button_visibility_set_up_ = true;
    }

    leading_button_container_->SetBounds(kSpacingBetweenImages, 0,
                                         web_app::kIconSize, fixed_height);

    trailing_button_container_->SetBounds(
        width() - kSpacingBetweenImages - web_app::kIconSize, 0,
        web_app::kIconSize, fixed_height);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    int host_view = available_size.width().is_bounded()
                        ? available_size.width().value()
                        : width();
    // Use a fixed height that guarantees to fit the screenshot with max ratio
    // and still show a clip for the next screenshot.
    const int fixed_height = base::checked_cast<int>(
        base::checked_cast<float>(host_view - image_padding_ * 2) /
        webapps::kMaximumScreenshotRatio);

    int width = 0;
    for (const auto& screenshot : screenshots_) {
      width += base::checked_cast<int>(
          screenshot.image.width() * (base::checked_cast<float>(fixed_height) /
                                      screenshot.image.height()));
    }
    return gfx::Size(width, fixed_height);
  }

 private:
  void OnScrollButtonClicked(ButtonType button_type) {
    DCHECK(image_inner_container_->children().size());

    int image_width =
        image_inner_container_->children().front()->bounds().width() +
        image_padding_;
    int container_width = image_container_->bounds().width();

    // Scroll past all the fully visible images
    int delta = image_width * (container_width / image_width);

    if (button_type == ButtonType::TRAILING) {
      delta = -delta;
    }

    const gfx::Rect& bounds = image_inner_container_->bounds();
    int x = bounds.x() + delta;

    // Bound the position so there is no empty space drawn before the first
    // image or after last image.
    x = std::min(x, 0);
    x = std::max(x, (container_width - image_carousel_full_width_));

    leading_button_->SetVisible(x < 0);

    trailing_button_->SetVisible(x + image_carousel_full_width_ >
                                 container_width);

    bounds_animator_->AnimateViewTo(
        image_inner_container_,
        gfx::Rect(x, bounds.y(), bounds.width(), bounds.height()));
  }

  std::vector<webapps::Screenshot> screenshots_;
  std::unique_ptr<views::BoundsAnimator> bounds_animator_;
  raw_ptr<views::View> image_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> image_inner_container_ = nullptr;
  std::vector<raw_ptr<views::ImageView>> image_views_;
  raw_ptr<views::View> leading_button_ = nullptr;
  raw_ptr<views::View> trailing_button_ = nullptr;
  raw_ptr<views::View> leading_button_container_ = nullptr;
  raw_ptr<views::View> trailing_button_container_ = nullptr;
  int image_carousel_full_width_ = 0;
  int image_padding_ = 0;
  bool trailing_button_visibility_set_up_ = false;
};

BEGIN_METADATA(ImageCarouselView)
END_METADATA

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

}  // namespace

namespace web_app {

void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    std::vector<webapps::Screenshot> screenshots,
    PwaInProductHelpState iph_state) {
  // Do not show the dialog if it is already being shown.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (!manager || manager->IsDialogActive()) {
    std::move(callback).Run(/*is_accepted=*/false, nullptr);
    return;
  }

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  PrefService* const prefs =
      Profile::FromBrowserContext(browser_context)->GetPrefs();

  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  gfx::ImageSkia icon_image(std::make_unique<WebAppInfoImageSource>(
                                kIconSize, install_info->icon_bitmaps.any),
                            gfx::Size(kIconSize, kIconSize));

  auto title = install_info->title;
  GURL start_url = install_info->start_url();
  std::u16string start_url_host_formatted_for_display =
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          start_url);

  const std::u16string description = gfx::TruncateString(
      install_info->description, webapps::kMaximumDescriptionLength,
      gfx::CHARACTER_BREAK);
  auto manifest_id = install_info->manifest_id();

  auto delegate = std::make_unique<WebAppInstallDialogDelegate>(
      web_contents, std::move(install_info), std::move(install_tracker),
      std::move(callback), std::move(iph_state), prefs, tracker,
      InstallDialogType::kDetailed);
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  std::unique_ptr<ui::DialogModel> dialog_model;
  if (base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)) {
    dialog_model =
        ui::DialogModel::Builder(std::move(delegate))
            .SetInternalName("WebAppDetailedInstallDialog")
            .SetTitle(l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_TITLE))
            .AddCustomField(
                std::make_unique<views::BubbleDialogModelHost::CustomView>(
                    WebAppIconNameAndOriginView::Create(icon_image, title,
                                                        start_url),
                    views::BubbleDialogModelHost::FieldType::kControl))
            .AddParagraph(
                ui::DialogModelLabel(description).set_is_secondary(),
                l10n_util::GetStringUTF16(
                    IDS_WEB_APP_DETAILED_INSTALL_DIALOG_DESCRIPTION_TITLE))
            .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                        delegate_weak_ptr),
                         ui::DialogModel::Button::Params().SetLabel(
                             l10n_util::GetStringUTF16(IDS_INSTALL)))
            .AddCancelButton(base::BindOnce(
                &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
            .SetCloseActionCallback(base::BindOnce(
                &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
            .SetDialogDestroyingCallback(base::BindOnce(
                &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
            .AddCustomField(
                std::make_unique<views::BubbleDialogModelHost::CustomView>(
                    std::make_unique<ImageCarouselView>(screenshots),
                    views::BubbleDialogModelHost::FieldType::kControl))
            .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
            .Build();
  } else {
    // TODO(crbug.com/341254289): Completely remove after Universal Install has
    // launched to 100% on Stable.
    dialog_model =
        ui::DialogModel::Builder(std::move(delegate))
            .SetInternalName("WebAppDetailedInstallDialog")
            .SetIcon(ui::ImageModel::FromImageSkia(icon_image))
            .SetTitle(title)
            .SetSubtitle(start_url_host_formatted_for_display)
            .AddParagraph(
                ui::DialogModelLabel(description).set_is_secondary(),
                l10n_util::GetStringUTF16(
                    IDS_WEB_APP_DETAILED_INSTALL_DIALOG_DESCRIPTION_TITLE))
            .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                        delegate_weak_ptr),
                         ui::DialogModel::Button::Params().SetLabel(
                             l10n_util::GetStringUTF16(IDS_INSTALL)))
            .AddCancelButton(base::BindOnce(
                &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
            .SetDialogDestroyingCallback(base::BindOnce(
                &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
            .AddCustomField(
                std::make_unique<views::BubbleDialogModelHost::CustomView>(
                    std::make_unique<ImageCarouselView>(screenshots),
                    views::BubbleDialogModelHost::FieldType::kControl))
            .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
            .Build();
  }
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);

  views::Widget* detailed_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  delegate_weak_ptr->StartObservingForPictureInPictureOcclusion(
      detailed_dialog_widget);
  base::RecordAction(base::UserMetricsAction("WebAppDetailedInstallShown"));

#if BUILDFLAG(IS_CHROMEOS)
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogShown().SetAppId(
          app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace web_app
