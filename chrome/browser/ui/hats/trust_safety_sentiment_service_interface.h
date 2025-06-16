// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_INTERFACE_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_INTERFACE_H_

#include <map>
#include <string>

#include "chrome/browser/download/download_item_warning_data.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"

namespace content {
class WebContents;
}

class TrustSafetySentimentServiceInterface {
 public:
  using PasswordProtectionUIType = safe_browsing::WarningUIType;
  using PasswordProtectionUIAction = safe_browsing::WarningAction;

  enum class FeatureArea {
    kIneligible = 0,
    kPrivacySettings = 1,
    kTrustedSurface = 2,
    kTransactions = 3,
    kSafetyCheck = 10,
    kPasswordCheck = 11,
    kBrowsingData = 12,
    kPrivacyGuide = 13,
    kControlGroup = 14,
    kSafeBrowsingInterstitial = 19,
    kDownloadWarningUI = 20,
    kPasswordProtectionUI = 21,
    kSafetyHubNotification = 22,
    kSafetyHubInteracted = 23,
    kMaxValue = kSafetyHubInteracted,
  };

  virtual ~TrustSafetySentimentServiceInterface() = default;

  // Called when the user opens an NTP. This allows the service to update its
  // eligibility logic, and potentially show a survey.
  virtual void OpenedNewTabPage() = 0;

  // Called when the user interacts with the privacy settings on
  // chrome://settings in |web_contents|. Interaction in this context could be
  // using a link row on the privacy settings card. Calling this allows the
  // service to monitor |web_contents| to determine if the user stays on
  // settings for the required time.
  virtual void InteractedWithPrivacySettings(
      content::WebContents* web_contents) = 0;

  // Called when the user runs safety check. This is immediately considered as a
  // trigger action.
  virtual void RanSafetyCheck() = 0;

  // Called when the user opens Page Info.
  virtual void PageInfoOpened() = 0;

  // Called when the user interacts in some way with Page Info.
  virtual void InteractedWithPageInfo() = 0;

  // Called when the user saves a password via the manage passwords UI. This is
  // the native UI shown when Chrome detects a password has been entered into
  // the web page.
  virtual void SavedPassword() = 0;

  // Called when the user closes Page Info. If Page Info was opened for the
  // target time, or the user interacted with it while it was open, a trigger
  // action is recorded.
  virtual void PageInfoClosed() = 0;

  // Called when the user visits chrome://settings/passwords. Calling this
  // allows the service to monitor |web_contents| to determine if the user
  // remains on settings after visiting the page for the required time.
  virtual void OpenedPasswordManager(content::WebContents* web_contents) = 0;

  // Called when the user saves a card through the native UI bubble shown after
  // the user uses a card on a website.
  virtual void SavedCard() = 0;

  // Called when the user runs password check.
  virtual void RanPasswordCheck() = 0;

  // Called when the user deletes data from Clear Browsing Data dialog.
  virtual void ClearedBrowsingData(
      browsing_data::BrowsingDataType datatype) = 0;

  // Called when the user finishes the privacy guide.
  virtual void FinishedPrivacyGuide() = 0;

  // Called when the user interacts with a safe browsing blocking page.
  virtual void InteractedWithSafeBrowsingInterstitial(
      bool did_proceed,
      safe_browsing::SBThreatType threat_type) = 0;

  // Called when the user completes terminal action within a download warning.
  // These actions can include: DISCARD, and PROCEED.
  virtual void InteractedWithDownloadWarningUI(
      DownloadItemWarningData::WarningSurface surface,
      DownloadItemWarningData::WarningAction action) = 0;

  // Called when user clicks to protect/reset/check their password on a password
  // protection UI. This triggers a survey if the user has not finished changing
  // their password after a certain period of time.
  virtual void ProtectResetOrCheckPasswordClicked(
      PasswordProtectionUIType ui_type) = 0;

  // Called when a user sees a password protection warning and decides to ignore
  // the warning, close the warning, or mark the warning as legitimate.
  virtual void PhishedPasswordUpdateNotClicked(
      PasswordProtectionUIType ui_type,
      PasswordProtectionUIAction action) = 0;

  // Called when a user finishes updating their phished password after seeing a
  // warning.
  virtual void PhishedPasswordUpdateFinished() = 0;

  // Triggers a survey for Safety Hub for the given feature area (visiting SH or
  // seeing a notification).
  virtual void TriggerSafetyHubSurvey(
      FeatureArea feature_area,
      std::map<std::string, bool> product_specific_data) = 0;
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_INTERFACE_H_
