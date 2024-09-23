// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40147906): Enable gn check once it learns about conditional
// includes.
#include "components/metrics/structured/structured_events.h"  // nogncheck
#include "components/metrics/structured/structured_metrics_client.h"  // nogncheck
#endif

namespace {

PWAConfirmationBubbleView* g_bubble_ = nullptr;

// Returns an ImageView containing the app icon.
std::unique_ptr<views::ImageView> CreateIconView(
    const web_app::WebAppInstallInfo& web_app_info) {
  constexpr int kIconSize = 48;
  gfx::ImageSkia image(std::make_unique<WebAppInfoImageSource>(
                           kIconSize, web_app_info.icon_bitmaps.any),
                       gfx::Size(kIconSize, kIconSize));

  auto icon_image_view = std::make_unique<views::ImageView>();
  icon_image_view->SetImage(ui::ImageModel::FromImageSkia(image));
  return icon_image_view;
}

#if BUILDFLAG(IS_CHROMEOS)
namespace cros_events = metrics::structured::events::v2::cr_os_events;

int64_t ToLong(web_app::WebAppInstallStatus web_app_install_status) {
  return static_cast<int64_t>(web_app_install_status);
}
#endif

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PWAConfirmationBubbleView,
                                      kInstallButton);
DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(PWAConfirmationBubbleView,
                                       kInstalledPWAEventId);

// static
bool PWAConfirmationBubbleView::IsShowing() {
  return g_bubble_;
}

// static
PWAConfirmationBubbleView* PWAConfirmationBubbleView::GetBubble() {
  return g_bubble_;
}

PWAConfirmationBubbleView::PWAConfirmationBubbleView(
    views::View* anchor_view,
    base::WeakPtr<content::WebContents> web_contents,
    PageActionIconView* pwa_install_icon_view,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    web_app::AppInstallationAcceptanceCallback callback,
    web_app::PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker)
    : LocationBarBubbleDelegateView(anchor_view, web_contents.get()),
      web_contents_(web_contents),
      pwa_install_icon_view_(pwa_install_icon_view),
      web_app_info_(std::move(web_app_info)),
      install_tracker_(std::move(install_tracker)),
      callback_(std::move(callback)),
      iph_state_(iph_state),
      prefs_(prefs),
      tracker_(tracker) {
  SetCloseOnMainFrameOriginNavigation(true);
  DCHECK(web_app_info_);
  DCHECK(prefs_);

  WidgetDelegate::SetShowCloseButton(true);
  WidgetDelegate::SetTitle(
      l10n_util::GetStringUTF16(IDS_INSTALL_TO_OS_LAUNCH_SURFACE_BUBBLE_TITLE));

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_INSTALL_PWA_BUTTON_LABEL));
  base::TrimWhitespace(web_app_info_->title, base::TRIM_ALL,
                       &web_app_info_->title);
  // PWAs should always be configured not to open in a browser tab.
  if (web_app_info_->user_display_mode.has_value()) {
    DCHECK_NE(*web_app_info_->user_display_mode,
              web_app::mojom::UserDisplayMode::kBrowser);
  }

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Use CONTROL insets, because the icon is non-text (see documentation for
  // DialogContentType).
  gfx::Insets margin_insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
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

  labels->AddChildView(
      web_app::CreateNameLabel(web_app_info_->title).release());
  labels->AddChildView(
      web_app::CreateOriginLabelFromStartUrl(web_app_info_->start_url(), false)
          .release());

  if (base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip) &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings)) {
    // This UI is only for prototyping and is not intended for shipping.
    DCHECK_EQ(features::kDesktopPWAsTabStripSettings.default_state,
              base::FEATURE_DISABLED_BY_DEFAULT);
    tabbed_window_checkbox_ = labels->AddChildView(
        std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
            IDS_BOOKMARK_APP_BUBBLE_OPEN_AS_TABBED_WINDOW)));
    tabbed_window_checkbox_->SetChecked(
        web_app_info_->user_display_mode ==
        web_app::mojom::UserDisplayMode::kTabbed);
  }

  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));

  SetHighlightedButton(pwa_install_icon_view_);

  CHECK(!g_bubble_);
  g_bubble_ = this;
}

PWAConfirmationBubbleView::~PWAConfirmationBubbleView() = default;

void PWAConfirmationBubbleView::OnWidgetInitialized() {
  auto* ok_button = GetOkButton();
  if (ok_button) {
    ok_button->SetProperty(views::kElementIdentifierKey,
                           PWAConfirmationBubbleView::kInstallButton);
  }
}

bool PWAConfirmationBubbleView::OnCloseRequested(
    views::Widget::ClosedReason close_reason) {
  base::UmaHistogramEnumeration("WebApp.InstallConfirmation.CloseReason",
                                close_reason);
  webapps::MlInstallUserResponse response;
  switch (close_reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      response = webapps::MlInstallUserResponse::kAccepted;
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kCancelButtonClicked:
    case views::Widget::ClosedReason::kEscKeyPressed:
      response = webapps::MlInstallUserResponse::kCancelled;
      break;
    case views::Widget::ClosedReason::kLostFocus:
    case views::Widget::ClosedReason::kUnspecified:
      // This is usually due to web contents navigation or tab changing.
      response = webapps::MlInstallUserResponse::kIgnored;
      break;
  }
  if (install_tracker_) {
    install_tracker_->ReportResult(response);
    install_tracker_.reset();
  }
  return LocationBarBubbleDelegateView::OnCloseRequested(close_reason);
}

views::View* PWAConfirmationBubbleView::GetInitiallyFocusedView() {
  return nullptr;
}

void PWAConfirmationBubbleView::WindowClosing() {
  DCHECK_EQ(g_bubble_, this);
  g_bubble_ = nullptr;

  if (pwa_install_icon_view_) {
    pwa_install_icon_view_->Update();
  }

  // If |web_app_info_| is populated, then the bubble was not accepted.
  if (web_app_info_) {
    base::RecordAction(base::UserMetricsAction("WebAppInstallCancelled"));
    const webapps::AppId app_id =
        web_app::GenerateAppIdFromManifestId(web_app_info_->manifest_id());
#if BUILDFLAG(IS_CHROMEOS)
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_Browser_AppInstallDialogResult()
            .SetWebAppInstallStatus(
                ToLong(web_app::WebAppInstallStatus::kCancelled))
            .SetAppId(app_id));
#endif  //  BUILDFLAG(IS_CHROMEOS)

    if (iph_state_ == web_app::PwaInProductHelpState::kShown) {
      web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_)
          .RecordIgnore(app_id, base::Time::Now());
    }
  } else {
    base::RecordAction(base::UserMetricsAction("WebAppInstallAccepted"));
  }

  if (callback_) {
    DCHECK(web_app_info_);
    std::move(callback_).Run(false, std::move(web_app_info_));
  }
}

bool PWAConfirmationBubbleView::Accept() {
  DCHECK(web_app_info_);
  web_app_info_->user_display_mode =
      tabbed_window_checkbox_ && tabbed_window_checkbox_->GetChecked()
          ? web_app::mojom::UserDisplayMode::kTabbed
          : web_app::mojom::UserDisplayMode::kStandalone;

  webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(web_app_info_->manifest_id());

#if BUILDFLAG(IS_CHROMEOS)
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::AppDiscovery_Browser_AppInstallDialogResult()
          .SetWebAppInstallStatus(
              ToLong(web_app::WebAppInstallStatus::kAccepted))
          .SetAppId(app_id));
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (iph_state_ == web_app::PwaInProductHelpState::kShown) {
    web_app::WebAppPrefGuardrails::GetForDesktopInstallIph(prefs_).RecordAccept(
        app_id);
    tracker_->NotifyEvent(feature_engagement::events::kDesktopPwaInstalled);
  }
  auto* ok_button = GetOkButton();
  auto* tracker_framework = ui::ElementTracker::GetFrameworkDelegate();
  auto* tracker_views = views::ElementTrackerViews::GetInstance();
  if (ok_button && tracker_framework && tracker_views) {
    tracker_framework->NotifyCustomEvent(
        tracker_views->GetElementForView(ok_button),
        PWAConfirmationBubbleView::kInstalledPWAEventId);
  }
  install_tracker_->ReportResult(webapps::MlInstallUserResponse::kAccepted);
  install_tracker_.reset();

  std::move(callback_).Run(true, std::move(web_app_info_));
  return true;
}

void PWAConfirmationBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->name = "PWAConfirmationBubbleView";
}

BEGIN_METADATA(PWAConfirmationBubbleView)
END_METADATA
