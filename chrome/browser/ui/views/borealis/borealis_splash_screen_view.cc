// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace {

borealis::BorealisSplashScreenView* g_delegate = nullptr;

const SkColor background_color = SkColorSetARGB(255, 53, 51, 50);
const SkColor text_color = SkColorSetARGB(255, 209, 208, 207);
const int icon_width = 250;
const int icon_height = 150;

static constexpr char logo_url[] =
    "https://store.cloudflare.steamstatic.com/public/shared/images/header/"
    "logo_steam.svg";

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("borealis_splash_logo_loader", R"(
      semantics {
        sender: "Steam Splash Screen"
        description:
          "Fetches image for Steam splash screen."
          " Data source is from a third party."
        trigger:
          "When Steam is launched."
        data: "URL of the image to be fetched."
        destination: OTHER
        destination_other: "Steam public assets server"
      })");

}  // namespace

namespace borealis {

// Declared in chrome/browser/ash/borealis/borealis_util.h.
void ShowBorealisSplashScreenView(Profile* profile) {
  return BorealisSplashScreenView::Show(profile);
}

void CloseBorealisSplashScreenView() {
  if (g_delegate) {
    g_delegate->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

void BorealisSplashScreenView::Show(Profile* profile) {
  if (!g_delegate) {
    auto delegate = std::make_unique<BorealisSplashScreenView>(profile);
    g_delegate = delegate.get();
    views::DialogDelegate::CreateDialogWidget(std::move(delegate), nullptr,
                                              nullptr);
    g_delegate->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey, ash::ShelfID(borealis::kInstallerAppId).Serialize());
  }
}

BorealisSplashScreenView::BorealisSplashScreenView(Profile* profile)
    : weak_factory_(this) {
  profile_ = profile;
  borealis::BorealisService::GetForProfile(profile_)
      ->WindowManager()
      .AddObserver(this);

  SetShowCloseButton(false);
  SetHasWindowSizeControls(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  views::LayoutProvider* provider = views::LayoutProvider::Get();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets().set_top(150).set_bottom(100), 50);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  set_use_custom_frame(true);
  SetBackground(
      views::CreateThemedSolidBackground(kColorBorealisSplashScreenBackground));

  set_corner_radius(15);
  set_fixed_width(600);

  views::BoxLayoutView* text_view =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  text_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  text_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  text_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::unique_ptr<views::Throbber> spinner =
      std::make_unique<views::Throbber>();
  spinner->Start();
  text_view->AddChildView(std::move(spinner));

  starting_label_ = text_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BOREALIS_SPLASHSCREEN_MESSAGE),
      views::Label::CustomFont{gfx::FontList({"Google Sans"}, gfx::Font::NORMAL,
                                             18, gfx::Font::Weight::NORMAL)}));
  starting_label_->SetEnabledColor(text_color);
  starting_label_->SetBackgroundColor(background_color);

  // Get logo path and add it to view.
  FetchLogo();
}

void BorealisSplashScreenView::OnSessionStarted() {
  DCHECK(GetWidget() != nullptr);
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void BorealisSplashScreenView::OnWindowManagerDeleted(
    borealis::BorealisWindowManager* window_manager) {
  DCHECK(window_manager ==
         &borealis::BorealisService::GetForProfile(profile_)->WindowManager());
  window_manager->RemoveObserver(this);
}

BorealisSplashScreenView::~BorealisSplashScreenView() {
  if (profile_)
    borealis::BorealisService::GetForProfile(profile_)
        ->WindowManager()
        .RemoveObserver(this);
  g_delegate = nullptr;
}

BorealisSplashScreenView* BorealisSplashScreenView::GetActiveViewForTesting() {
  return g_delegate;
}

void BorealisSplashScreenView::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(kColorBorealisSplashScreenBackground);
  const SkColor foreground_color =
      color_provider->GetColor(kColorBorealisSplashScreenForeground);
  GetBubbleFrameView()->SetBackgroundColor(background_color);
  starting_label_->SetBackgroundColor(background_color);
  starting_label_->SetEnabledColor(foreground_color);
}

void BorealisSplashScreenView::CreateImageView(std::string image_data) {
  transcoder_.reset();

  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>();
  constexpr gfx::Size kRegularImageSize(icon_width, icon_height);
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
  image_view->SetImage(image.AsImageSkia());
  image_view->SetImageSize(kRegularImageSize);

  std::unique_ptr<views::BoxLayoutView> image_container =
      std::make_unique<views::BoxLayoutView>();
  // The logo has some blank space on the right side. Adding an inset to the
  // left to offset this.
  // TODO(b/231395059): Host a version of the logo without the blank space.
  // image_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  image_container->SetInsideBorderInsets(gfx::Insets().set_left(40));
  image_container->AddChildView(std::move(image_view));
  AddChildViewAt(std::move(image_container), 0);

  // The logo height doesn't seem to get taken into account
  // so the splash screen gets displayed too low. Subtracting half of the
  // icon's height to compensate for this.
  gfx::Rect rect = GetBoundsInScreen();
  g_delegate->GetWidget()->SetBounds(gfx::Rect(
      rect.x(), rect.y() - icon_height / 2, rect.width(), rect.height()));

  // This is the last method to run so calling Show here.
  g_delegate->GetWidget()->Show();
}

void BorealisSplashScreenView::FetchLogo() {
  auto* image_fetcher =
      ImageFetcherServiceFactory::GetForKey(profile_->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kDiskCacheOnly);

  image_fetcher->FetchImageData(
      GURL(logo_url),
      base::BindOnce(&BorealisSplashScreenView::OnImageFetched,
                     weak_factory_.GetWeakPtr()),
      image_fetcher::ImageFetcherParams(traffic_annotation,
                                        "BorealisSplashscreen"));
}

void BorealisSplashScreenView::OnImageFetched(
    const std::string& image_data,
    const image_fetcher::RequestMetadata& request_metadata) {
  const base::FilePath splash_logo_path("");
  transcoder_ = std::make_unique<apps::SvgIconTranscoder>(profile_);
  transcoder_->Transcode(
      image_data, std::move(splash_logo_path),
      gfx::Size(icon_width, icon_height),
      base::BindOnce(&BorealisSplashScreenView::CreateImageView,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace borealis
