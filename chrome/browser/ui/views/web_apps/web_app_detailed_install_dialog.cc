// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
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
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

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
  explicit ImageCarouselView(
      base::WeakPtr<web_app::WebAppScreenshotFetcher> fetcher)
      : fetcher_(fetcher) {
    // Use a fill layout to draw the buttons container on
    // top of the image carousel.
    SetUseDefaultFillLayout(true);

    image_padding_ = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
    image_container_ = AddChildView(std::make_unique<views::View>());

    image_inner_container_ = image_container_->AddChildView(
        std::make_unique<views::BoxLayoutView>());
    image_inner_container_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    image_inner_container_->SetProperty(
        views::kElementIdentifierKey,
        web_app::kDetailedInstallDialogImageContainer);

    // Create the loading icons that have the same width as the calculated
    // downloaded size of the screenshots, with similar padding as when images
    // are loaded.
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

  // Start fetching screenshots after the throbbers have been added to the
  // widget.
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
    // If the bitmap being downloaded is empty, do not attempt to draw it in a
    // loading area.
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

    // Use a fixed height that guarantees to fit the screenshot with max
    // ratio and still show a clip for the next screenshot.
    const gfx::Size current_image_size(screenshot.GetImage().Width(),
                                       screenshot.GetImage().Height());
    image_view->SetImageSize(
        {GetScaledWidthBasedOnThrobberHeight(current_image_size),
         GetFullThrobberHeight()});
    if (label) {
      image_view->GetViewAccessibility().SetName(label.value());
    }

    // Destroy the `throbber view` and replace it with the image view.
    image_inner_container_->RemoveChildViewT(
        image_inner_container_->children()[index]);
    image_inner_container_->AddChildViewAt(std::move(image_view), index);

    InvalidateLayout();
  }

  void Layout(PassKey) override {
    image_container_->SetBounds(0, 0, width(), height());

    // Only setup the initial visibility once based on container width & max
    // screenshot ratio, the visibility is later updated by
    // `OnScrollButtonClicked` based on image carousel animation.
    if (!trailing_button_visibility_set_up_) {
      image_carousel_full_width_ =
          image_inner_container_->GetPreferredSize().width();
      trailing_button_->SetVisible(image_carousel_full_width_ > width());
      trailing_button_visibility_set_up_ = true;
    }

    // Center both of the scroll buttons to the middle of the image container.
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

    // Scroll past all the fully visible images
    int delta = image_width * (container_width / image_width);

    if (button_type == ButtonType::kTrailing) {
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

  // Return the scaled width based on whether it is limited by the fixed height
  // of the throbber container, or the maximum w/h ratio of screenshots.
  int GetScaledWidthBasedOnThrobberHeight(const gfx::Size& size) {
    const int throbber_height = GetFullThrobberHeight();
    CHECK_GE(size.height(), 0) << "screenshot cannot have an empty height";
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

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;
#endif

}  // namespace

namespace web_app {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDetailedInstallDialogImageContainer);

void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> install_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    base::WeakPtr<web_app::WebAppScreenshotFetcher> fetcher,
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

  DialogImageInfo dialog_image_info =
      install_info->GetIconBitmapsForSecureSurfaces();
  gfx::ImageSkia icon_image(
      std::make_unique<WebAppInfoImageSource>(
          kIconSize, std::move(dialog_image_info.bitmaps)),
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
  dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("WebAppDetailedInstallDialog")
          .SetTitle(l10n_util::GetStringUTF16(IDS_INSTALL_PWA_DIALOG_TITLE))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  WebAppIconNameAndOriginView::Create(
                      icon_image, title.value(), start_url,
                      dialog_image_info.is_maskable),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .AddParagraph(
              ui::DialogModelLabel(description).set_is_secondary(),
              l10n_util::GetStringUTF16(
                  IDS_WEB_APP_DETAILED_INSTALL_DIALOG_DESCRIPTION_TITLE))
          .AddOkButton(base::BindOnce(&WebAppInstallDialogDelegate::OnAccept,
                                      delegate_weak_ptr),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(IDS_INSTALL))
                           .SetId(WebAppInstallDialogDelegate::
                                      kPwaInstallDialogInstallButton))
          .AddCancelButton(base::BindOnce(
              &WebAppInstallDialogDelegate::OnCancel, delegate_weak_ptr))
          .SetCloseActionCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnClose, delegate_weak_ptr))
          .SetDialogDestroyingCallback(base::BindOnce(
              &WebAppInstallDialogDelegate::OnDestroyed, delegate_weak_ptr))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::make_unique<ImageCarouselView>(fetcher),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build();
  auto dialog = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kChild);
  views::Widget* detailed_dialog_widget =
      constrained_window::ShowWebModalDialogViews(dialog.release(),
                                                  web_contents);
  if (IsWidgetCurrentSizeSmallerThanPreferredSize(detailed_dialog_widget)) {
    delegate_weak_ptr->CloseDialogAsIgnored();
    return;
  }
  delegate_weak_ptr->OnWidgetShownStartTracking(detailed_dialog_widget);

  base::RecordAction(base::UserMetricsAction("WebAppDetailedInstallShown"));

#if BUILDFLAG(IS_CHROMEOS)
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogShown().SetAppId(
          app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace web_app
