// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int getDaysToExpiration(base::Time expiration) {
  // TODO(crbug.com/1446230): Apply DST corrections.
  const base::Time midnight_today = base::Time::Now().LocalMidnight();
  const base::Time midnight_expiration = expiration.LocalMidnight();
  return (midnight_expiration - midnight_today).InDays();
}

}  // namespace

// Expected URL types for `UrlIdentity::CreateFromUrl()`.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};

constexpr UrlIdentity::FormatOptions kUrlIdentityOptions{
    .default_options = {UrlIdentity::DefaultFormatOptions::
                            kOmitSchemePathAndTrivialSubdomains}};

CookieControlsBubbleViewController::CookieControlsBubbleViewController(
    CookieControlsBubbleView* bubble_view,
    content_settings::CookieControlsController* controller,
    content::WebContents* web_contents)
    : bubble_view_(bubble_view), controller_(controller->AsWeakPtr()) {
  controller_observation_.Observe(controller);
  bubble_view_->UpdateSubtitle(GetSubjectUrlName(web_contents));

  content_view_ =
      bubble_view_->AddChildView(std::make_unique<CookieControlsContentView>());

  SetFeedbackButtonPressedCallback();
}

CookieControlsBubbleViewController::~CookieControlsBubbleViewController() =
    default;

void CookieControlsBubbleViewController::OnStatusChanged(
    CookieControlsStatus status,
    CookieControlsEnforcement enforcement,
    base::Time expiration) {
  switch (status) {
    case CookieControlsStatus::kEnabled:
      bubble_view_->UpdateTitle(l10n_util::GetStringUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE));
      content_view_->UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE),
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY));
      content_view_->SetFeedbackSectionVisibility(false);
      break;
    case CookieControlsStatus::kDisabledForSite: {
      bool is_permanent_exception =
          getDaysToExpiration(expiration) == 0 || expiration == base::Time();
      bubble_view_->UpdateTitle(l10n_util::GetStringUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE));
      content_view_->UpdateContentLabels(
          is_permanent_exception
              ? l10n_util::GetStringUTF16(
                    IDS_COOKIE_CONTROLS_BUBBLE_PERMANENT_ALLOWED_TITLE)
              : l10n_util::GetPluralStringFUTF16(
                    IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_TITLE,
                    getDaysToExpiration(expiration)),
          l10n_util::GetStringUTF16(
              is_permanent_exception
                  ? IDS_COOKIE_CONTROLS_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION
                  : IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_DESCRIPTION_TODAY));
      content_view_->SetFeedbackSectionVisibility(true);
    } break;
    case CookieControlsStatus::kDisabled:
    case CookieControlsStatus::kUninitialized:
      NOTREACHED();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CookieControlsBubbleViewController::OnSitesCountChanged(
    int allowed_sites,
    int blocked_sites) {
  // TODO(1446230): Implement OnSitesCountChanged
}

void CookieControlsBubbleViewController::OnBreakageConfidenceLevelChanged(
    CookieControlsBreakageConfidenceLevel level) {
  // TODO(1446230): Implement OnBreakageConfidenceLevelChanged.
}

void CookieControlsBubbleViewController::SetFeedbackButtonPressedCallback() {
  feedback_button_callback_ =
      content_view_->RegisterFeedbackButtonPressedCallback(base::BindRepeating(
          &CookieControlsBubbleViewController::OnFeedbackButtonPressed,
          base::Unretained(this)));
}

void CookieControlsBubbleViewController::OnFeedbackButtonPressed() {
  // TODO(crbug.com/1446230): Handle Feedback button press.
}

std::u16string CookieControlsBubbleViewController::GetSubjectUrlName(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();

  return UrlIdentity::CreateFromUrl(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             nav_entry->GetURL(), kUrlIdentityAllowedTypes, kUrlIdentityOptions)
      .name;
}
