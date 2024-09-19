// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/borealis/borealis_beta_badge.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace borealis {

namespace {
borealis::BorealisSplashScreenView* g_delegate = nullptr;

constexpr int kCornerRadius = 24;
constexpr int kOuterPadding = 48;
constexpr int kInnerPadding = 40;
}  // namespace

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
    g_delegate->UpdateColors();
    g_delegate->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey, ash::ShelfID(borealis::kClientAppId).Serialize());
    // Override the widget to be dark-mode permanently.
    // This UI has custom colors to match Steam's and those are close to ash's
    // dark mode.
    g_delegate->GetWidget()->SetColorModeOverride(
        {ui::ColorProviderKey::ColorMode::kDark});
  }
  g_delegate->GetWidget()->Show();
}

BorealisSplashScreenView::BorealisSplashScreenView(Profile* profile)
    : start_tick_(base::TimeTicks::Now()), weak_factory_(this) {
  profile_ = profile;
  borealis::BorealisServiceFactory::GetForProfile(profile_)
      ->WindowManager()
      .AddObserver(this);

  SetTitle(IDS_BOREALIS_SPLASHSCREEN_TITLE);
  SetShowCloseButton(false);
  SetHasWindowSizeControls(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_margins(gfx::Insets(kOuterPadding));
  set_corner_radius(kCornerRadius);
  set_use_custom_frame(true);
  SetBackground(
      views::CreateThemedSolidBackground(kColorBorealisSplashScreenBackground));

  views::LayoutProvider* provider = views::LayoutProvider::Get();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(kInnerPadding);
  SetLayoutManager(std::move(layout));

  views::BoxLayoutView* upper_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  upper_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  upper_container->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  upper_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  title_label_ = upper_container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BOREALIS_SPLASHSCREEN_TITLE),
      // TODO(b/284389804): Use TypographyToken::kCrosDisplay7
      views::Label::CustomFont{gfx::FontList({"Google Sans", "Roboto"},
                                             gfx::Font::NORMAL, 18,
                                             gfx::Font::Weight::MEDIUM)}));

  upper_container->AddChildView(std::make_unique<BorealisBetaBadge>());

  views::BoxLayoutView* lower_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  lower_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  lower_container->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  lower_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  views::Throbber* spinner =
      lower_container->AddChildView(std::make_unique<views::Throbber>());
  spinner->Start();

  starting_label_ =
      lower_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_BOREALIS_SPLASHSCREEN_MESSAGE),
          // TODO(b/284389804): Use TypographyToken::kCrosBody1
          views::Label::CustomFont{gfx::FontList({"Google Sans", "Roboto"},
                                                 gfx::Font::NORMAL, 14,
                                                 gfx::Font::Weight::NORMAL)}));
}

void BorealisSplashScreenView::OnSessionStarted() {
  DCHECK(GetWidget() != nullptr);
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  RecordBorealisStartupTimeToFirstWindowHistogram(base::TimeTicks::Now() -
                                                  start_tick_);
}

void BorealisSplashScreenView::OnWindowManagerDeleted(
    borealis::BorealisWindowManager* window_manager) {
  DCHECK(window_manager ==
         &borealis::BorealisServiceFactory::GetForProfile(profile_)
              ->WindowManager());
  window_manager->RemoveObserver(this);
}

BorealisSplashScreenView::~BorealisSplashScreenView() {
  if (profile_) {
    borealis::BorealisServiceFactory::GetForProfile(profile_)
        ->WindowManager()
        .RemoveObserver(this);
  }
  g_delegate = nullptr;
}

BorealisSplashScreenView* BorealisSplashScreenView::GetActiveViewForTesting() {
  return g_delegate;
}

void BorealisSplashScreenView::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();
  // The splash screen defies dark/light mode, so re-update the colour after
  // views changes it.
  UpdateColors();
}

bool BorealisSplashScreenView::ShouldShowWindowTitle() const {
  return false;
}

void BorealisSplashScreenView::UpdateColors() {
  const auto* const color_provider = GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(kColorBorealisSplashScreenBackground);
  const SkColor foreground_color =
      color_provider->GetColor(kColorBorealisSplashScreenForeground);
  GetBubbleFrameView()->SetBackgroundColor(background_color);
  title_label_->SetBackgroundColor(background_color);
  title_label_->SetEnabledColor(foreground_color);
  starting_label_->SetBackgroundColor(background_color);
  starting_label_->SetEnabledColor(foreground_color);
}

}  // namespace borealis
