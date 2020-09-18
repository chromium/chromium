// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"

#include <utility>

#include "base/i18n/message_formatter.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
constexpr char kDeviceTypeForCheckbox[] = "computer";
#else
constexpr char kDeviceTypeForCheckbox[] = "other";
#endif

PWAConfirmationBubbleView* g_bubble_ = nullptr;

bool g_auto_accept_pwa_for_testing = false;

// Returns an ImageView containing the app icon.
std::unique_ptr<views::ImageView> CreateIconView(
    const WebApplicationInfo& web_app_info) {
  constexpr int kIconSize = 48;
  gfx::ImageSkia image(std::make_unique<WebAppInfoImageSource>(
                           kIconSize, web_app_info.icon_bitmaps_any),
                       gfx::Size(kIconSize, kIconSize));

  auto icon_image_view = std::make_unique<views::ImageView>();
  icon_image_view->SetImage(image);
  return icon_image_view;
}

// Returns a label containing the app name.
std::unique_ptr<views::Label> CreateNameLabel(const base::string16& name) {
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
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY);

  origin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Elide from head to prevent origin spoofing.
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, so explicitly disable multiline.
  origin_label->SetMultiLine(false);

  return origin_label;
}

}  // namespace

// static
bool PWAConfirmationBubbleView::IsShowing() {
  return g_bubble_;
}

// static
PWAConfirmationBubbleView* PWAConfirmationBubbleView::GetBubbleForTesting() {
  return g_bubble_;
}

PWAConfirmationBubbleView::PWAConfirmationBubbleView(
    views::View* anchor_view,
    views::Button* highlight_button,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      web_app_info_(std::move(web_app_info)),
      callback_(std::move(callback)),
      run_on_os_login_(nullptr) {
  DCHECK(web_app_info_);

  WidgetDelegate::SetShowCloseButton(true);
  WidgetDelegate::SetTitle(
      l10n_util::GetStringUTF16(IDS_INSTALL_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE));

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_INSTALL_PWA_BUTTON_LABEL));
  base::TrimWhitespace(web_app_info_->title, base::TRIM_ALL,
                       &web_app_info_->title);
  // PWAs should always be configured to open in a window.
  DCHECK(web_app_info_->open_as_window);

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Use CONTROL insets, because the icon is non-text (see documentation for
  // DialogContentType).
  gfx::Insets margin_insets = layout_provider->GetDialogInsetsForContentType(
      views::CONTROL, views::CONTROL);
  set_margins(margin_insets);

  int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      icon_label_spacing));

  AddChildView(CreateIconView(*web_app_info_).release());

  views::View* labels = new views::View();
  AddChildView(labels);
  labels->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  labels->AddChildView(CreateNameLabel(web_app_info_->title).release());
  labels->AddChildView(
      CreateOriginLabel(url::Origin::Create(web_app_info_->start_url))
          .release());

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip)) {
    // This UI is only for prototyping and is not intended for shipping.
    DCHECK_EQ(features::kDesktopPWAsTabStrip.default_state,
              base::FEATURE_DISABLED_BY_DEFAULT);
    tabbed_window_checkbox_ = labels->AddChildView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TABBED_WINDOW)));
    tabbed_window_checkbox_->SetChecked(
        web_app_info_->enable_experimental_tabbed_window);
  }

  // TODO(crbug.com/897302): This is an experimental UI added to prototype
  // The PWA Run on OS Login feature, final design is yet to be decided.
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    // TODO(crbug.com/897302): Detect the type of device and supply the proper
    // constant for the string.
    run_on_os_login_ = labels->AddChildView(std::make_unique<views::Checkbox>(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_INSTALL_PWA_RUN_ON_OS_LOGIN_LABEL),
            kDeviceTypeForCheckbox)));
  }

  chrome::RecordDialogCreation(chrome::DialogIdentifier::PWA_CONFIRMATION);

  SetHighlightedButton(highlight_button);
}

PWAConfirmationBubbleView::~PWAConfirmationBubbleView() = default;

bool PWAConfirmationBubbleView::OnCloseRequested(
    views::Widget::ClosedReason close_reason) {
  base::UmaHistogramEnumeration("WebApp.InstallConfirmation.CloseReason",
                                close_reason);
  return LocationBarBubbleDelegateView::OnCloseRequested(close_reason);
}

views::View* PWAConfirmationBubbleView::GetInitiallyFocusedView() {
  return nullptr;
}

void PWAConfirmationBubbleView::WindowClosing() {
  DCHECK_EQ(g_bubble_, this);
  g_bubble_ = nullptr;
  if (callback_) {
    DCHECK(web_app_info_);
    std::move(callback_).Run(false, std::move(web_app_info_));
  }
}

bool PWAConfirmationBubbleView::Accept() {
  DCHECK(web_app_info_);
  if (tabbed_window_checkbox_) {
    web_app_info_->enable_experimental_tabbed_window =
        tabbed_window_checkbox_->GetChecked();
  }

  // User opt-in in checkbox is passed via the web_app_info structure to the
  // underlying PWA install code.
  // The definition of run_on_os_login_ is dependent on
  // features::kDesktopPWAsRunOnOsLogin being enabled.
  if (run_on_os_login_)
    web_app_info_->run_on_os_login = run_on_os_login_->GetChecked();

  std::move(callback_).Run(true, std::move(web_app_info_));
  return true;
}

views::Checkbox*
PWAConfirmationBubbleView::GetRunOnOsLoginCheckboxForTesting() {
  return run_on_os_login_;
}

namespace chrome {

void ShowPWAInstallBubble(content::WebContents* web_contents,
                          std::unique_ptr<WebApplicationInfo> web_app_info,
                          AppInstallationAcceptanceCallback callback) {
  if (g_bubble_)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAnchorView(
          PageActionIconType::kPwaInstall);
  PageActionIconView* icon =
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kPwaInstall);

  g_bubble_ = new PWAConfirmationBubbleView(
      anchor_view, icon, std::move(web_app_info), std::move(callback));

  views::BubbleDialogDelegateView::CreateBubble(g_bubble_)->Show();

  if (g_auto_accept_pwa_for_testing)
    g_bubble_->AcceptDialog();

  if (icon) {
    icon->Update();
    DCHECK(icon->GetVisible());
  }
}

void SetAutoAcceptPWAInstallConfirmationForTesting(bool auto_accept) {
  g_auto_accept_pwa_for_testing = auto_accept;
}

}  // namespace chrome
