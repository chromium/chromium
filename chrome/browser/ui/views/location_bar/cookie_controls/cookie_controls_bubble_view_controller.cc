// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"

#include "base/check_is_test.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_util.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/favicon/core/favicon_service.h"
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
  if (enabled) {
    return features::IsChromeRefresh2023() ? views::kEyeRefreshIcon
                                           : views::kEyeIcon;
  } else {
    return features::IsChromeRefresh2023() ? views::kEyeCrossedRefreshIcon
                                           : views::kEyeCrossedIcon;
  }
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

  bubble_view_->InitContentView(std::make_unique<CookieControlsContentView>());
  bubble_view_->InitReloadingView(InitReloadingView(web_contents));

  FetchFaviconFrom(web_contents);
  SetCallbacks();

  bubble_view_->GetReloadingView()->SetVisible(false);
  bubble_view_->GetContentView()->SetVisible(true);
  bubble_view_->GetContentView()->SetAccessibleRole(ax::mojom::Role::kAlert);
}

void CookieControlsBubbleViewController::OnUserClosedContentView() {
  if (!controller_->HasCookieBlockingChangedForSite()) {
    controller_observation_.Reset();
    bubble_view_->CloseWidget();
    return;
  }

  if (!web_contents_) {
    return;
  }

  web_contents_->GetController().SetNeedsReload();
  web_contents_->GetController().LoadIfNecessary();

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
  if (latest_blocking_status_ == CookieBlocking3pcdStatus::kNotIn3pcd) {
    bubble_title = IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE;
    if (is_permanent_exception) {
      label_title = l10n_util::GetStringUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_PERMANENT_ALLOWED_TITLE);
      label_description =
          IDS_COOKIE_CONTROLS_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION;
    } else {
      label_title = l10n_util::GetPluralStringFUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_TITLE,
          content_settings::CookieControlsUtil::GetDaysToExpiration(
              expiration));
      label_description =
          IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_DESCRIPTION_TODAY;
    }
  } else {
    bubble_title = IDS_TRACKING_PROTECTION_BUBBLE_TITLE;
    if (is_permanent_exception ||
        enforcement == CookieControlsEnforcement::kEnforcedByCookieSetting) {
      label_title = l10n_util::GetStringUTF16(
          IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE);
      label_description =
          IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION;
    } else {
      label_title = l10n_util::GetPluralStringFUTF16(
          latest_blocking_status_ == CookieBlocking3pcdStatus::kAll
              ? IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_TITLE
              : IDS_TRACKING_PROTECTION_BUBBLE_LIMITING_RESTART_TITLE,
          content_settings::CookieControlsUtil::GetDaysToExpiration(
              expiration));
      label_description =
          IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION;
    }
  }

  bubble_view_->UpdateTitle(l10n_util::GetStringUTF16(bubble_title));
  bubble_view_->GetContentView()->UpdateContentLabels(
      label_title, l10n_util::GetStringUTF16(label_description));
  bubble_view_->GetContentView()->SetToggleIsOn(true);
  bubble_view_->GetContentView()->SetToggleIcon(GetToggleIcon(true));
}

void CookieControlsBubbleViewController::ApplyThirdPartyCookiesBlockedState() {
  auto default_exception_expiration =
      content_settings::features::kUserBypassUIExceptionExpiration.Get();
  int label_title, label_description;
  if (latest_blocking_status_ == CookieBlocking3pcdStatus::kNotIn3pcd) {
    label_title = IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE;
    label_description =
        default_exception_expiration.is_zero()
            ? IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_PERMANENT
            : IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY;
  } else {
    label_title = IDS_TRACKING_PROTECTION_BUBBLE_TITLE;
    label_description =
        IDS_TRACKING_PROTECTION_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_PERMANENT;
  }

  bubble_view_->UpdateTitle(l10n_util::GetStringUTF16(label_title));
  bubble_view_->GetContentView()->UpdateContentLabels(
      l10n_util::GetStringUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE),
      l10n_util::GetStringUTF16(label_description));
  bubble_view_->GetContentView()->SetToggleIsOn(false);
  bubble_view_->GetContentView()->SetToggleIcon(GetToggleIcon(false));
}

CookieControlsBubbleViewController::~CookieControlsBubbleViewController() =
    default;

void CookieControlsBubbleViewController::OnStatusChanged(
    CookieControlsStatus status,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration) {
  latest_status_ = status;
  latest_blocking_status_ = blocking_status;

  switch (status) {
    case CookieControlsStatus::kEnabled:
      ApplyThirdPartyCookiesBlockedState();
      break;
    case CookieControlsStatus::kDisabledForSite:
      ApplyThirdPartyCookiesAllowedState(enforcement, expiration);
      break;
    case CookieControlsStatus::kDisabled:
    case CookieControlsStatus::kUninitialized:
      bubble_view_->CloseWidget();
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (enforcement) {
    case CookieControlsEnforcement::kNoEnforcement:
      bubble_view_->GetContentView()->SetContentLabelsVisible(true);
      bubble_view_->GetContentView()->SetFeedbackSectionVisibility(
          status == CookieControlsStatus::kDisabledForSite);
      bubble_view_->GetContentView()->SetToggleVisible(true);
      bubble_view_->GetContentView()->SetEnforcedIconVisible(false);
      break;
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
      bubble_view_->CloseWidget();
      break;
    case CookieControlsEnforcement::kEnforcedByPolicy:
    case CookieControlsEnforcement::kEnforcedByExtension:
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      // In 3PCD, tell the user if they allowed the current site in settings.
      bubble_view_->GetContentView()->SetContentLabelsVisible(
          enforcement == CookieControlsEnforcement::kEnforcedByCookieSetting &&
          blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd);
      bubble_view_->GetContentView()->SetFeedbackSectionVisibility(false);
      bubble_view_->GetContentView()->SetToggleVisible(false);
      bubble_view_->GetContentView()->SetEnforcedIcon(
          content_settings::CookieControlsUtil::GetEnforcedIcon(enforcement),
          l10n_util::GetStringUTF16(
              content_settings::CookieControlsUtil::GetEnforcedTooltipTextId(
                  enforcement))),
          bubble_view_->GetContentView()->SetEnforcedIconVisible(true);
      break;
  }
  // If we're in 3PCD, update toggle label based on status.
  if (latest_blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd) {
    int label;
    if (latest_status_ == CookieControlsStatus::kDisabledForSite) {
      label = IDS_TRACKING_PROTECTION_BUBBLE_COOKIES_ALLOWED_LABEL;
    } else {
      label = latest_blocking_status_ == CookieBlocking3pcdStatus::kAll
                  ? IDS_TRACKING_PROTECTION_BUBBLE_COOKIES_BLOCKED_LABEL
                  : IDS_TRACKING_PROTECTION_BUBBLE_COOKIES_LIMITED_LABEL;
    }
    bubble_view_->GetContentView()->SetToggleLabel(
        l10n_util::GetStringUTF16(label));
  }
}

void CookieControlsBubbleViewController::OnSitesCountChanged(
    int allowed_third_party_sites_count,
    int blocked_third_party_sites_count) {
  // We don't surface site counts in the UB bubble for 3PCD instead we will set
  // the label in `OnStatusChange`.
  if (latest_blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd) {
    return;
  }
  std::u16string label;
  switch (latest_status_) {
    case CookieControlsStatus::kEnabled:
      label = l10n_util::GetPluralStringFUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_BLOCKED_SITES_COUNT,
          blocked_third_party_sites_count);
      break;
    case CookieControlsStatus::kDisabledForSite:
      label = l10n_util::GetPluralStringFUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_ALLOWED_SITES_COUNT,
          allowed_third_party_sites_count);
      break;
    case CookieControlsStatus::kDisabled:
    case CookieControlsStatus::kUninitialized:
      // If this happens, it is transitory and and can be ignored.
      break;
    default:
      NOTREACHED();
      break;
  }
  bubble_view_->GetContentView()->SetToggleLabel(label);
}

void CookieControlsBubbleViewController::OnBreakageConfidenceLevelChanged(
    CookieControlsBreakageConfidenceLevel level) {
  // TODO(1446230): Implement OnBreakageConfidenceLevelChanged.
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
    bool allow_third_party_cookies) {
  if (allow_third_party_cookies) {
    base::RecordAction(base::UserMetricsAction(
        "CookieControls.Bubble.AllowThirdPartyCookies"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "CookieControls.Bubble.BlockThirdPartyCookies"));
  }
  controller_->OnCookieBlockingEnabledForSite(!allow_third_party_cookies);
  bubble_view_->GetContentView()->NotifyAccessibilityEvent(
      ax::mojom::Event::kAlert, true);
}

void CookieControlsBubbleViewController::OnFeedbackButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("CookieControls.Bubble.SendFeedback"));
  chrome::ShowFeedbackPage(
      chrome::FindBrowserWithTab(web_contents_.get()),
      chrome::kFeedbackSourceCookieControls,
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
  progress_bar->SetPreferredCornerRadii(absl::nullopt);
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
