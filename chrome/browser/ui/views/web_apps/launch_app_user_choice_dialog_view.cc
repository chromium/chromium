// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/launch_app_user_choice_dialog_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

bool g_default_remember_selection = false;

// TODO(https://crbug.com/1248757): Reconcile the code here, and the
// code in the PWA install dialog, and URL Handler picker.
// Returns a label containing the app name.
std::unique_ptr<views::Label> CreateNameLabel(const std::u16string& name) {
  auto name_label = std::make_unique<views::Label>(
      name, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_PRIMARY);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  name_label->SetElideBehavior(gfx::ELIDE_TAIL);
  return name_label;
}

std::unique_ptr<views::Label> CreateOriginLabel(const url::Origin& origin) {
  auto origin_label = std::make_unique<views::Label>(
      FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
  origin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Elide from head to prevent origin spoofing.
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, so explicitly disable multiline.
  origin_label->SetMultiLine(false);

  return origin_label;
}

}  // namespace

namespace web_app {

void LaunchAppUserChoiceDialogView::SetDefaultRememberSelectionForTesting(
    bool remember_selection) {
  g_default_remember_selection = remember_selection;
}

LaunchAppUserChoiceDialogView::LaunchAppUserChoiceDialogView(
    Profile* profile,
    const web_app::AppId& app_id,
    chrome::WebAppLaunchAcceptanceCallback close_callback)
    : profile_(profile),
      app_id_(app_id),
      close_callback_(std::move(close_callback)) {}

LaunchAppUserChoiceDialogView::~LaunchAppUserChoiceDialogView() = default;

void LaunchAppUserChoiceDialogView::Init() {
  SetModalType(ui::MODAL_TYPE_NONE);
#if !BUILDFLAG(IS_CHROMEOS)
  SetTitle(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
#endif
  SetShowCloseButton(true);
  SetCanResize(false);
  set_draggable(true);

  SetAcceptCallback(base::BindOnce(&LaunchAppUserChoiceDialogView::OnAccepted,
                                   base::Unretained(this)));

  SetCancelCallback(base::BindOnce(&LaunchAppUserChoiceDialogView::OnCanceled,
                                   base::Unretained(this)));

  SetCloseCallback(base::BindOnce(&LaunchAppUserChoiceDialogView::OnClosed,
                                  base::Unretained(this)));
  InitChildViews();
}

void LaunchAppUserChoiceDialogView::OnAccepted() {
  RunCloseCallback(
      /*allowed=*/true,
      /*remember_user_choice=*/remember_selection_checkbox_->GetChecked());
}

void LaunchAppUserChoiceDialogView::OnCanceled() {
  RunCloseCallback(
      /*allowed=*/false,
      /*remember_user_choice=*/remember_selection_checkbox_->GetChecked());
}

void LaunchAppUserChoiceDialogView::OnClosed() {
  switch (GetWidget()->closed_reason()) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      OnAccepted();
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      OnCanceled();
      break;
    default:
      RunCloseCallback(/*allowed=*/false, /*remember_user_choice=*/false);
      break;
  }
}

void LaunchAppUserChoiceDialogView::InitChildViews() {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int vertical_single_distance = layout_provider->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_single_distance));

  auto above_app_info_view = CreateAboveAppInfoView();
  if (above_app_info_view)
    AddChildView(std::move(above_app_info_view));

  // Add the app info, which will look like:
  // +-------------------------------------------------------------------+
  // |      |    app short name                                          |
  // | icon |                                                            |
  // |      |    origin                                                  |
  // +-------------------------------------------------------------------+
  {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(profile_);
    web_app::WebAppRegistrar& registrar = provider->registrar();
    auto app_info_view = std::make_unique<views::View>();
    int icon_label_spacing = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
    app_info_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        icon_label_spacing));

    provider->icon_manager().ReadIcons(
        app_id_, IconPurpose::ANY,
        provider->registrar().GetAppDownloadedIconSizesAny(app_id_),
        base::BindOnce(&LaunchAppUserChoiceDialogView::OnIconsRead,
                       weak_ptr_factory_.GetWeakPtr()));
    icon_image_view_ =
        app_info_view->AddChildView(std::make_unique<views::ImageView>());
    icon_image_view_->SetCanProcessEventsWithinSubtree(false);
    icon_image_view_->SetImageSize(
        gfx::Size(web_app::kWebAppIconSmall, web_app::kWebAppIconSmall));

    auto app_name_publisher_view = std::make_unique<views::View>();
    app_name_publisher_view->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    app_name_publisher_view->AddChildView(
        CreateNameLabel(base::UTF8ToUTF16(registrar.GetAppShortName(app_id_)))
            .release());
    app_name_publisher_view->AddChildView(
        CreateOriginLabel(
            url::Origin::Create(registrar.GetAppStartUrl(app_id_)))
            .release());
    app_info_view->AddChildView(std::move(app_name_publisher_view));

    AddChildView(std::move(app_info_view));
  }

  auto below_app_info_view = CreateBelowAppInfoView();
  if (below_app_info_view)
    AddChildView(std::move(below_app_info_view));

  remember_selection_checkbox_ = AddChildView(
      std::make_unique<views::Checkbox>(GetRememberChoiceString()));
  remember_selection_checkbox_->SetChecked(g_default_remember_selection);
  remember_selection_checkbox_->SetMultiLine(true);
}

void LaunchAppUserChoiceDialogView::RunCloseCallback(
    bool allowed,
    bool remember_user_choice) {
  if (close_callback_) {
    // Give the stack a chance to unwind in case `close_callback_` deletes
    // `this`.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(close_callback_), allowed,
                                  remember_user_choice));
  }
}

void LaunchAppUserChoiceDialogView::OnIconsRead(
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty() || !icon_image_view_)
    return;

  gfx::Size image_size{web_app::kWebAppIconSmall, web_app::kWebAppIconSmall};
  auto imageSkia =
      gfx::ImageSkia(std::make_unique<WebAppInfoImageSource>(
                         web_app::kWebAppIconSmall, std::move(icon_bitmaps)),
                     image_size);
  icon_image_view_->SetImage(imageSkia);
}

BEGIN_METADATA(LaunchAppUserChoiceDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace web_app
