// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

borealis::BorealisSplashScreenView* g_delegate = nullptr;

constexpr int kIconSize = 40;

gfx::Image ReadImageFile(std::string dlc_path) {
  base::FilePath image_path =
      base::FilePath(dlc_path.append("/splash_logo.png"));
  std::string image_data;

  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read borealis logo from disk.";
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
}

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
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  layout->set_minimum_cross_axis_size(kIconSize);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kText));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_use_custom_frame(true);
  SetBackground(
      views::CreateThemedSolidBackground(kColorBorealisSplashScreenBackground));

  // Get logo path and add it to view.
  borealis::GetDlcPath(base::BindOnce(&BorealisSplashScreenView::OnGetRootPath,
                                      weak_factory_.GetWeakPtr()));

  views::View* text_view = AddChildView(std::make_unique<views::View>());
  std::unique_ptr<views::BoxLayout> layout2 =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  text_view->SetLayoutManager(std::move(layout2));

  std::unique_ptr<views::BoxLayout> text_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical);
  text_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  text_view->SetLayoutManager(std::move(text_layout));
  set_corner_radius(10);
  set_fixed_width(380);

  steam_label_ = text_view->AddChildView(std::make_unique<views::Label>(
      u"STEAM FOR CHROMEBOOK",
      views::Label::CustomFont{gfx::FontList({"Google Sans"}, gfx::Font::NORMAL,
                                             18, gfx::Font::Weight::NORMAL)}));

  starting_label_ = text_view->AddChildView(std::make_unique<views::Label>(
      u"STARTING UP...",
      views::Label::CustomFont{gfx::FontList({"Google Sans"}, gfx::Font::NORMAL,
                                             18, gfx::Font::Weight::NORMAL)}));
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
  steam_label_->SetBackgroundColor(background_color);
  steam_label_->SetEnabledColor(foreground_color);
  starting_label_->SetBackgroundColor(background_color);
  starting_label_->SetEnabledColor(foreground_color);
}

void BorealisSplashScreenView::OnGetRootPath(const std::string& path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadImageFile, path),
      base::BindOnce(&BorealisSplashScreenView::CreateImageView,
                     weak_factory_.GetWeakPtr()));
}

void BorealisSplashScreenView::CreateImageView(gfx::Image image) {
  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>();
  constexpr gfx::Size kRegularImageSize(kIconSize, kIconSize);
  image_view->SetImage(image.AsImageSkia());
  image_view->SetImageSize(kRegularImageSize);
  AddChildViewAt(std::move(image_view), 0);
  Layout();
}

}  // namespace borealis
