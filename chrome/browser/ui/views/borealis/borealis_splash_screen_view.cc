// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include "ash/public/cpp/window_properties.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"

namespace {

borealis::BorealisSplashScreenView* g_delegate = nullptr;

const SkColor background_color = SkColorSetARGB(255, 53, 51, 50);
const SkColor text_color = SkColorSetARGB(255, 209, 208, 207);
const int icon_width = 252;
const int icon_height = 77;

}  // namespace

namespace borealis {

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
  g_delegate->GetWidget()->Show();
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
      gfx::Insets().set_top(150).set_bottom(100), 100);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  set_use_custom_frame(true);
  SetBackground(
      views::CreateThemedSolidBackground(kColorBorealisSplashScreenBackground));

  set_corner_radius(15);
  set_fixed_width(600);

  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>();
  constexpr gfx::Size kRegularImageSize(icon_width, icon_height);
  image_view->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_BOREALIS_SPLASH));
  image_view->SetImageSize(kRegularImageSize);

  std::unique_ptr<views::BoxLayoutView> image_container =
      std::make_unique<views::BoxLayoutView>();
  image_container->AddChildView(std::move(image_view));
  AddChildViewAt(std::move(image_container), 0);

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

}  // namespace borealis
