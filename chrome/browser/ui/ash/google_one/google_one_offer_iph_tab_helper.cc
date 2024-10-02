// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/google_one/google_one_offer_iph_tab_helper.h"

#include <optional>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/google_one/google_one_offer_iph_tab_helper_constants.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace {

class DriveIphTabHelperNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DriveIphTabHelperNotificationDelegate(feature_engagement::Tracker* tracker,
                                        Profile* profile)
      : tracker_(tracker), profile_(profile) {}

  // message_center::NotificationDelegate:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (!button_index.has_value() ||
        button_index.value() != kGetPerkButtonIndex) {
      return;
    }

    ash::NewWindowDelegate::GetPrimary()->OpenUrl(
        GURL(kGoogleOneOfferUrl),
        ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
        ash::NewWindowDelegate::Disposition::kNewForegroundTab);

    tracker_->NotifyEvent(kIPHGoogleOneOfferNotificationGetPerkEventName);

    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, kIPHGoogleOneOfferNotificationId);
  }

  void Close(bool by_user) override {
    // If it's closed by a user, let's assume that the user intent is to dismiss
    // a notification. Record it as an IPH event. Note that the above
    // `NotificationDisplayService::Close` call does not set by_user=true.
    if (by_user) {
      tracker_->NotifyEvent(kIPHGoogleOneOfferNotificationDismissEventName);
    }

    // Tracker::Dismissed is to notify IPH framework that this IPH has ended.
    // This is different from the above dismiss event.
    tracker_->Dismissed(
        feature_engagement::kIPHGoogleOneOfferNotificationFeature);
  }

 private:
  friend class message_center::NotificationDelegate;
  ~DriveIphTabHelperNotificationDelegate() override = default;

  raw_ptr<feature_engagement::Tracker, DanglingUntriaged> tracker_;
  raw_ptr<Profile> profile_;
};

message_center::Notification CreateGoogleOneOfferNotification(
    feature_engagement::Tracker* tracker,
    Profile* profile) {
  std::string notification_display_source =
      base::GetFieldTrialParamValueByFeature(
          feature_engagement::kIPHGoogleOneOfferNotificationFeature,
          kNotificationDisplaySourceParamName);
  if (notification_display_source.empty()) {
    notification_display_source = kFallbackNotificationDisplaySource;
  }

  std::string notification_title = base::GetFieldTrialParamValueByFeature(
      feature_engagement::kIPHGoogleOneOfferNotificationFeature,
      kNotificationTitleParamName);
  if (notification_title.empty()) {
    notification_title = kFallbackNotificationTitle;
  }

  std::string notification_message = base::GetFieldTrialParamValueByFeature(
      feature_engagement::kIPHGoogleOneOfferNotificationFeature,
      kNotificationMessageParamName);
  if (notification_message.empty()) {
    notification_message = kFallbackNotificationMessage;
  }

  std::string get_perk_button_title = base::GetFieldTrialParamValueByFeature(
      feature_engagement::kIPHGoogleOneOfferNotificationFeature,
      kGetPerkButtonTitleParamName);
  if (get_perk_button_title.empty()) {
    get_perk_button_title = kFallbackGetPerkButtonTitle;
  }

  message_center::RichNotificationData rich_notification_data;
  message_center::ButtonInfo get_perk_button_info;
  get_perk_button_info.title = base::UTF8ToUTF16(get_perk_button_title);
  rich_notification_data.buttons.push_back(get_perk_button_info);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kIPHGoogleOneOfferNotifierId,
      ash::NotificationCatalogName::kIPHGoogleOneOffer);

  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kIPHGoogleOneOfferNotificationId, base::UTF8ToUTF16(notification_title),
      base::UTF8ToUTF16(notification_message),
      base::UTF8ToUTF16(notification_display_source), GURL(), notifier_id,
      rich_notification_data,
      base::MakeRefCounted<DriveIphTabHelperNotificationDelegate>(tracker,
                                                                  profile),
      chromeos::kRedeemIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

constexpr char kGoogleDriveDomainName[] = "drive.google.com";
constexpr char kGooglePhotosDomainName[] = "photos.google.com";

}  // namespace

GoogleOneOfferIphTabHelper::~GoogleOneOfferIphTabHelper() = default;

void GoogleOneOfferIphTabHelper::PrimaryPageChanged(content::Page& page) {
  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.DomainIs(kGoogleDriveDomainName) &&
      !url.DomainIs(kGooglePhotosDomainName)) {
    return;
  }

  // Google One offer is eligible for a device, not for an account. Do not show
  // a notification if a device is enrolled.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (profile->IsGuestSession()) {
    return;
  }

  if (profile->IsChild()) {
    return;
  }

  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return;
  }

  if (!ash::ProfileHelper::IsOwnerProfile(profile)) {
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);

  if (!tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHGoogleOneOfferNotificationFeature)) {
    return;
  }

  const message_center::Notification notification =
      CreateGoogleOneOfferNotification(tracker, profile);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification, nullptr);
}

GoogleOneOfferIphTabHelper::GoogleOneOfferIphTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<GoogleOneOfferIphTabHelper>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(GoogleOneOfferIphTabHelper);
