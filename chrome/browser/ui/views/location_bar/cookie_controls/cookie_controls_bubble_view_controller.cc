// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"

#include "base/check_is_test.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_util.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/favicon/core/favicon_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "cookie_controls_bubble_view_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

using TrackingProtectionFeature = ::content_settings::TrackingProtectionFeature;
using FeatureType = ::content_settings::TrackingProtectionFeatureType;

constexpr int kProgressBarHeight = 3;

// Unique identifier within the CookieControlsBubbleView hierarchy.
constexpr int kFaviconID = 1;

// Expected URL types for `UrlIdentity::CreateFromUrl()`.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};

constexpr UrlIdentity::FormatOptions kUrlIdentityOptions{
    .default_options = {UrlIdentity::DefaultFormatOptions::
                            kOmitSchemePathAndTrivialSubdomains}};

const gfx::VectorIcon& GetToggleIcon(bool enabled) {
  return enabled ? views::kEyeRefreshIcon : views::kEyeCrossedRefreshIcon;
}

}  // namespace

CookieControlsBubbleViewController::CookieControlsBubbleViewController(
    CookieControlsBubbleView* bubble_view,
    content_settings::CookieControlsController* controller,
    content::WebContents* web_contents)
    : bubble_view_(bubble_view),
      controller_(controller->AsWeakPtr()),
      web_contents_(web_contents->GetWeakPtr()) {
  controller_observation_.Observe(controller);
  bubble_view_->UpdateSubtitle(GetSubjectUrlName(web_contents));

  bubble_view_->InitContentView(std::make_unique<CookieControlsContentView>(
      controller->ShowActFeatures()));
  bubble_view_->InitReloadingView(InitReloadingView(web_contents));

  FetchFaviconFrom(web_contents);
  SetCallbacks();

  bubble_view_->GetReloadingView()->SetVisible(false);
  bubble_view_->GetContentView()->SetVisible(true);
  bubble_view_->GetContentView()->GetViewAccessibility().SetRole(
      ax::mojom::Role::kAlert);
}

void CookieControlsBubbleViewController::OnUserClosedContentView() {
  if (!controller_->HasUserChangedCookieBlockingForSite()) {
    controller_observation_.Reset();
    bubble_view_->CloseWidget();
    return;
  }

  if (!web_contents_) {
    return;
  }

  web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);

  SwitchToReloadingView();
}

void CookieControlsBubbleViewController::SwitchToReloadingView() {
  bubble_view_->SwitchToReloadingView();
  bubble_view_->GetReloadingView()->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringFUTF16(IDS_COOKIE_CONTROLS_BUBBLE_RELOADING_LABEL,
                                 GetSubjectUrlName(web_contents_.get())));
  bubble_view_->GetReloadingView()->RequestFocus();

  // Set a timeout for how long the reloading view is shown for.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &CookieControlsBubbleViewController::OnReloadingViewTimeout,
          weak_factory_.GetWeakPtr()),
      content_settings::features::kUserBypassUIReloadBubbleTimeout.Get());
}

void CookieControlsBubbleViewController::OnFaviconFetched(
    const favicon_base::FaviconImageResult& result) const {
  bubble_view_->UpdateFaviconImage(result.image, kFaviconID);
}

void CookieControlsBubbleViewController::ApplyThirdPartyCookiesAllowedState(
    CookieControlsEnforcement enforcement,
    base::Time expiration) {
  bool is_permanent_exception = expiration == base::Time();
  std::u16string label_title;
  int bubble_title, label_description;
  if (is_permanent_exception ||
      enforcement == CookieControlsEnforcement::kEnforcedByCookieSetting) {
    label_title = l10n_util::GetStringUTF16(
        IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE);
    label_description =
        IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION;
  } else {
    label_title = l10n_util::GetPluralStringFUTF16(
        blocking_status_ == CookieBlocking3pcdStatus::kLimited
            ? IDS_TRACKING_PROTECTION_BUBBLE_LIMITING_RESTART_TITLE
            : IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_TITLE,
        content_settings::CookieControlsUtil::GetDaysToExpiration(expiration));
    label_description =
        IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION;
  }
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kTrackingProtection3pcdUx) &&
      blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd) {
    bubble_title = IDS_TRACKING_PROTECTION_BUBBLE_TITLE;
  } else {
    bubble_title = IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE;
  }

  bubble_view_->UpdateTitle(l10n_util::GetStringUTF16(bubble_title));
  bubble_view_->GetContentView()->UpdateContentLabels(
      label_title, l10n_util::GetStringUTF16(label_description));
  // ACT feature toggle matches protections state (off when protections off).
  bubble_view_->GetContentView()->SetToggleIsOn(
      !controller_->ShowActFeatures());
}

void CookieControlsBubbleViewController::ApplyThirdPartyCookiesBlockedState() {
  int label_title = IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE;
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kTrackingProtection3pcdUx) &&
      blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd) {
    label_title = IDS_TRACKING_PROTECTION_BUBBLE_TITLE;
  } else if (blocking_status_ == CookieBlocking3pcdStatus::kLimited) {
    label_title = IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_LIMITED_TITLE;
  }
  bubble_view_->UpdateTitle(l10n_util::GetStringUTF16(label_title));
  bubble_view_->GetContentView()->UpdateContentLabels(
      l10n_util::GetStringUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE),
      l10n_util::GetStringUTF16(
          IDS_TRACKING_PROTECTION_BUBBLE_SITE_NOT_WORKING_DESCRIPTION));
  // ACT feature toggle matches protections state (on when protections on).
  bubble_view_->GetContentView()->SetToggleIsOn(controller_->ShowActFeatures());
}

void CookieControlsBubbleViewController::FillViewForThirdPartyCookies(
    TrackingProtectionFeature cookies_feature,
    base::Time expiration) {
  if (protections_on_) {
    ApplyThirdPartyCookiesBlockedState();
  } else {
    ApplyThirdPartyCookiesAllowedState(cookies_feature.enforcement, expiration);
  }
  bubble_view_->GetContentView()->SetToggleIcon(
      GetToggleIcon(!protections_on_));
  bubble_view_->GetContentView()->SetCookiesLabel(
      GetStatusLabel(cookies_feature.status));
  switch (cookies_feature.enforcement) {
    case CookieControlsEnforcement::kNoEnforcement:
      bubble_view_->GetContentView()->SetContentLabelsVisible(true);
      bubble_view_->GetContentView()->SetFeedbackSectionVisibility(
          !protections_on_);
      bubble_view_->GetContentView()->SetToggleVisible(true);
      bubble_view_->GetContentView()->SetEnforcedIconVisible(false);
      break;
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
      bubble_view_->CloseWidget();
      break;
    case CookieControlsEnforcement::kEnforcedByPolicy:
    case CookieControlsEnforcement::kEnforcedByExtension:
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      bubble_view_->GetContentView()->SetContentLabelsVisible(
          cookies_feature.enforcement ==
          CookieControlsEnforcement::kEnforcedByCookieSetting);
      bubble_view_->GetContentView()->SetFeedbackSectionVisibility(false);
      bubble_view_->GetContentView()->SetToggleVisible(false);
      bubble_view_->GetContentView()->SetEnforcedIcon(
          content_settings::CookieControlsUtil::GetEnforcedIcon(
              cookies_feature.enforcement),
          content_settings::CookieControlsUtil::GetEnforcedTooltip(
              cookies_feature.enforcement)),
          bubble_view_->GetContentView()->SetEnforcedIconVisible(true);
      break;
  }
  bubble_view_->GetContentView()->PreferredSizeChanged();
}

std::u16string CookieControlsBubbleViewController::GetStatusLabel(
    content_settings::TrackingProtectionBlockingStatus blocking_status) {
  switch (blocking_status) {
    case content_settings::TrackingProtectionBlockingStatus::kAllowed:
      return l10n_util::GetStringUTF16(
          IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE);
    case content_settings::TrackingProtectionBlockingStatus::kBlocked:
      return l10n_util::GetStringUTF16(
          IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE);
    case content_settings::TrackingProtectionBlockingStatus::kLimited:
      return l10n_util::GetStringUTF16(
          IDS_TRACKING_PROTECTION_BUBBLE_3PC_LIMITED_SUBTITLE);
    default:
      return {};
  }
}

CookieControlsBubbleViewController::~CookieControlsBubbleViewController() =
    default;

void CookieControlsBubbleViewController::OnStatusChanged(
    bool controls_visible,
    bool protections_on,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration,
    std::vector<content_settings::TrackingProtectionFeature> features) {
  protections_on_ = protections_on;
  blocking_status_ = blocking_status;

  if (!controls_visible || features.empty()) {
    bubble_view_->CloseWidget();
    return;
  }
  if (controller_->ShowActFeatures()) {
    FillViewForTrackingProtection(enforcement, expiration, features);
  } else {
    // The legacy UI only supports 3PC blocking.
    CHECK(features[0].feature_type == FeatureType::kThirdPartyCookies);
    FillViewForThirdPartyCookies(features[0], expiration);
  }
}

void CookieControlsBubbleViewController::FillViewForTrackingProtection(
    CookieControlsEnforcement enforcement,
    base::Time expiration,
    std::vector<content_settings::TrackingProtectionFeature> features) {
  // Fill description strings and toggle state.
  if (protections_on_) {
    ApplyThirdPartyCookiesBlockedState();
  } else {
    ApplyThirdPartyCookiesAllowedState(enforcement, expiration);
  }

  std::vector<content_settings::TrackingProtectionFeature> managed_features;

  // TODO(http://b/344856056): Only show user bypass when the toggle is visible
  // (the user can control protections). Remove this variable once the UB
  // visibility is updated.
  bool show_toggle;

  // Handle enforced and unenforced feature states separately
  std::vector<TrackingProtectionFeature>::iterator it;
  for (it = features.begin(); it != features.end(); it++) {
    if (it->enforcement == CookieControlsEnforcement::kNoEnforcement) {
      show_toggle = true;
      bubble_view_->GetContentView()->AddFeatureRow(*it, protections_on_);
    } else {
      managed_features.push_back(*it);
    }
  }

  if (managed_features.size() > 0) {
    // TODO(http://b/352066532): Support multiple enforcements in managed
    // section.
    bubble_view_->GetContentView()->AddManagedSectionForEnforcement(
        enforcement);
    bubble_view_->GetContentView()->SetManagedSeparatorVisible(show_toggle);
  }
  // Fill managed feature rows
  for (it = managed_features.begin(); it != managed_features.end(); it++) {
    bubble_view_->GetContentView()->AddFeatureRow(*it, protections_on_);
  }

  // If there are features the user can control, display toggle.
  bubble_view_->GetContentView()->SetToggleVisible(show_toggle);
  bubble_view_->GetContentView()->SetContentLabelsVisible(show_toggle);

  // Show the feedback link if the user disabled protections
  bubble_view_->GetContentView()->SetFeedbackSectionVisibility(
      !protections_on_ &&
      enforcement == CookieControlsEnforcement::kNoEnforcement);

  bubble_view_->GetContentView()->PreferredSizeChanged();
}

void CookieControlsBubbleViewController::
    OnFinishedPageReloadWithChangedSettings() {
  // TODO: Log a UserMetricsAction here to count completed page reloads once we
  // have confidence that this callback is properly scoped.  See
  // https://crrev.com/c/4925330 for context.
  CloseBubble();
}

void CookieControlsBubbleViewController::OnReloadingViewTimeout() {
  base::RecordAction(
      base::UserMetricsAction("CookieControls.Bubble.ReloadingTimeout"));
  CloseBubble();
}

void CookieControlsBubbleViewController::CloseBubble() {
  controller_observation_.Reset();
  bubble_view_->CloseWidget();
  // View destruction is call asynchronously from the bubble being closed, so we
  // invalidate the weak pointers here to avoid callbacks happening after
  // the bubble is closed and before this class is destroyed.
  weak_factory_.InvalidateWeakPtrs();
}

void CookieControlsBubbleViewController::SetCallbacks() {
  on_user_closed_content_view_callback_ =
      bubble_view_->RegisterOnUserClosedContentViewCallback(base::BindRepeating(
          &CookieControlsBubbleViewController::OnUserClosedContentView,
          base::Unretained(this)));

  toggle_button_callback_ =
      bubble_view_->GetContentView()->RegisterToggleButtonPressedCallback(
          base::BindRepeating(
              &CookieControlsBubbleViewController::OnToggleButtonPressed,
              base::Unretained(this)));

  feedback_button_callback_ =
      bubble_view_->GetContentView()->RegisterFeedbackButtonPressedCallback(
          base::BindRepeating(
              &CookieControlsBubbleViewController::OnFeedbackButtonPressed,
              base::Unretained(this)));
}

void CookieControlsBubbleViewController::OnToggleButtonPressed(
    bool toggled_on) {
  // Protections are on iff the toggle is on in the ACT features UI or off in
  // the 3PC-only UI.
  bool protections_on = controller_->ShowActFeatures() == toggled_on;
  if (!protections_on) {
    base::RecordAction(base::UserMetricsAction(
        "CookieControls.Bubble.AllowThirdPartyCookies"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "CookieControls.Bubble.BlockThirdPartyCookies"));
  }
  controller_->SetUserChangedCookieBlockingForSite(true);
  // Set the toggle ON when protections are ON (cookies are blocked).
  controller_->OnCookieBlockingEnabledForSite(protections_on);
  bubble_view_->GetContentView()->NotifyAccessibilityEvent(
      ax::mojom::Event::kAlert, true);
}

void CookieControlsBubbleViewController::OnFeedbackButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("CookieControls.Bubble.SendFeedback"));
  chrome::ShowFeedbackPage(
      chrome::FindBrowserWithTab(web_contents_.get()),
      feedback::kFeedbackSourceCookieControls,
      /*description_template=*/std::string(),
      l10n_util::GetStringUTF8(
          IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_FORM_PLACEHOLDER),
      "cookie-controls",
      /*extra_diagnostics=*/std::string());
}

std::unique_ptr<views::View>
CookieControlsBubbleViewController::InitReloadingView(
    content::WebContents* web_contents) {
  auto* provider = ChromeLayoutProvider::Get();
  const int vertical_margin =
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  const int side_margin =
      provider->GetInsetsMetric(views::INSETS_DIALOG).left();

  auto reloading_view = std::make_unique<views::View>();
  reloading_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto progress_bar = std::make_unique<views::ProgressBar>();
  progress_bar->SetPreferredHeight(kProgressBarHeight);
  progress_bar->SetPreferredCornerRadii(std::nullopt);
  progress_bar->SetValue(-1);

  auto reloading_content = std::make_unique<views::View>();
  reloading_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  reloading_content->SetProperty(views::kMarginsKey,
                                 gfx::Insets::VH(vertical_margin, side_margin));

  auto favicon = std::make_unique<NonAccessibleImageView>();
  favicon->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, 0, 0, side_margin));
  favicon->SetID(kFaviconID);
  reloading_content->AddChildView(std::move(favicon));

  reloading_content->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_COOKIE_CONTROLS_BUBBLE_RELOADING_LABEL,
                                 GetSubjectUrlName(web_contents))));

  reloading_view->AddChildView(std::move(progress_bar));
  reloading_view->AddChildView(std::move(reloading_content));

  return reloading_view;
}

void CookieControlsBubbleViewController::FetchFaviconFrom(
    content::WebContents* web_contents) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  favicon::FaviconService* const favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  // FaviconService might be nullptr in unit tests.
  if (!favicon_service) {
    CHECK_IS_TEST();
    return;
  }

  favicon_service->GetFaviconImageForPageURL(
      web_contents->GetLastCommittedURL(),
      base::BindOnce(&CookieControlsBubbleViewController::OnFaviconFetched,
                     weak_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);
}

std::u16string CookieControlsBubbleViewController::GetSubjectUrlName(
    content::WebContents* web_contents) const {
  if (subject_url_name_for_testing_.has_value()) {
    return subject_url_name_for_testing_.value();
  }
  CHECK(web_contents);
  return UrlIdentity::CreateFromUrl(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             web_contents->GetLastCommittedURL(), kUrlIdentityAllowedTypes,
             kUrlIdentityOptions)
      .name;
}

void CookieControlsBubbleViewController::SetSubjectUrlNameForTesting(
    const std::u16string& name) {
  subject_url_name_for_testing_ = name;
  bubble_view_->UpdateSubtitle(GetSubjectUrlName(/*web_contents=*/nullptr));
}
