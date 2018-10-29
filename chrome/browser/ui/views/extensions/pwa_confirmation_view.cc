// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/pwa_confirmation_view.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/web_app_info_image_source.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/origin.h"

namespace {

constexpr int kPWAConfirmationViewIconSize = 48;

bool g_auto_accept_pwa_for_testing = false;

}  // namespace

PWAConfirmationView::PWAConfirmationView(
    const WebApplicationInfo& web_app_info,
    chrome::AppInstallationAcceptanceCallback callback)
    : web_app_info_(web_app_info), callback_(std::move(callback)) {
  base::TrimWhitespace(web_app_info_.title, base::TRIM_ALL,
                       &web_app_info_.title);
  // PWAs should always be configured to open in a window.
  DCHECK(web_app_info_.open_as_window);

  InitializeView();

  chrome::RecordDialogCreation(chrome::DialogIdentifier::PWA_CONFIRMATION);

  if (g_auto_accept_pwa_for_testing)
    Accept();
}

PWAConfirmationView::~PWAConfirmationView() {}

gfx::Size PWAConfirmationView::CalculatePreferredSize() const {
  int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);

  gfx::Size size = views::DialogDelegateView::CalculatePreferredSize();
  size.SetToMin(gfx::Size(bubble_width - margins().width(), size.height()));
  return size;
}

ui::ModalType PWAConfirmationView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 PWAConfirmationView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_INSTALL_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE);
}

bool PWAConfirmationView::ShouldShowCloseButton() const {
  return false;
}

void PWAConfirmationView::WindowClosing() {
  if (callback_)
    std::move(callback_).Run(false, web_app_info_);
}

bool PWAConfirmationView::Accept() {
  std::move(callback_).Run(true, web_app_info_);
  return true;
}

base::string16 PWAConfirmationView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_INSTALL_PWA_BUTTON_LABEL
                                       : IDS_CANCEL);
}

namespace {

// Returns an ImageView containing the app icon.
std::unique_ptr<views::ImageView> CreateIconView(
    const std::vector<WebApplicationInfo::IconInfo>& icons) {
  gfx::ImageSkia image(
      std::make_unique<WebAppInfoImageSource>(kPWAConfirmationViewIconSize,
                                              icons),
      gfx::Size(kPWAConfirmationViewIconSize, kPWAConfirmationViewIconSize));

  auto icon_image_view = std::make_unique<views::ImageView>();
  icon_image_view->SetImage(image);
  return icon_image_view;
}

// Returns a label containing the app name.
std::unique_ptr<views::Label> CreateNameLabel(const base::string16& name) {
  auto name_label = std::make_unique<views::Label>(
      name, CONTEXT_BODY_TEXT_LARGE, views::style::TextStyle::STYLE_PRIMARY);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  name_label->SetElideBehavior(gfx::ELIDE_TAIL);
  return name_label;
}

std::unique_ptr<views::Label> CreateOriginLabel(const url::Origin& origin) {
  auto origin_label = std::make_unique<views::Label>(
      FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
      CONTEXT_BODY_TEXT_SMALL, STYLE_SECONDARY);

  origin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Elide from head to prevent origin spoofing.
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, so explicitly disable multiline.
  origin_label->SetMultiLine(false);

  return origin_label;
}

}  // namespace

void PWAConfirmationView::InitializeView() {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Use CONTROL insets, because the icon is non-text (see documentation for
  // DialogContentType).
  gfx::Insets margin_insets = layout_provider->GetDialogInsetsForContentType(
      views::CONTROL, views::CONTROL);
  set_margins(margin_insets);

  int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(), icon_label_spacing));

  AddChildView(CreateIconView(web_app_info_.icons).release());

  views::View* labels = new views::View();
  AddChildView(labels);
  labels->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  labels->AddChildView(CreateNameLabel(web_app_info_.title).release());
  labels->AddChildView(
      CreateOriginLabel(url::Origin::Create(web_app_info_.app_url)).release());
}

namespace chrome {

void ShowPWAInstallDialog(content::WebContents* web_contents,
                          const WebApplicationInfo& web_app_info,
                          AppInstallationAcceptanceCallback callback) {
  constrained_window::ShowWebModalDialogViews(
      new PWAConfirmationView(web_app_info, std::move(callback)), web_contents);
}

void SetAutoAcceptPWAInstallDialogForTesting(bool auto_accept) {
  g_auto_accept_pwa_for_testing = auto_accept;
}

}  // namespace chrome
