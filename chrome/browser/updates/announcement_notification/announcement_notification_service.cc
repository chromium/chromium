// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"    // nogncheck
#include "chrome/browser/profiles/profile_attributes_storage.h"  // nogncheck
#include "chrome/browser/profiles/profile_manager.h"             // nogncheck
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Default value for Finch parameter |kVersion|.
const int kInvalidVersion = -1;

// Whether the announcement notification is shown.
static bool notification_shown = false;

}  // namespace

class AnnouncementNotificationServiceImpl
    : public AnnouncementNotificationService {
 public:
  AnnouncementNotificationServiceImpl(Profile* profile,
                                      PrefService* pref_service,
                                      std::unique_ptr<Delegate> delegate,
                                      base::Clock* clock)
      : profile_(profile),
        pref_service_(pref_service),
        delegate_(std::move(delegate)),
        clock_(clock),
        skip_first_run_(true),
        skip_new_profile_(false),
        skip_display_(false),
        remote_version_(kInvalidVersion),
        require_signout_(false),
        show_one_(false) {
    DCHECK(delegate_);
    DCHECK(profile_);

    if (!IsFeatureEnabled())
      return;

    // By default do nothing in first run.
    skip_first_run_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kSkipFirstRun, true);
    skip_new_profile_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kSkipNewProfile, false);
    skip_display_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kSkipDisplay, false);
    remote_version_ = base::GetFieldTrialParamByFeatureAsInt(
        kAnnouncementNotification, kVersion, kInvalidVersion);
    require_signout_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kRequireSignout, false);
    show_one_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kShowOneAllProfiles, false);
    remote_url_ = base::GetFieldTrialParamValueByFeature(
        kAnnouncementNotification, kAnnouncementUrl);

    bool success = base::Time::FromUTCString(
        base::GetFieldTrialParamValueByFeature(kAnnouncementNotification,
                                               kSkipFirstRunAfterTime)
            .c_str(),
        &skip_first_run_after_);
    if (!success)
      skip_first_run_after_ = base::Time();
  }

  AnnouncementNotificationServiceImpl(
      const AnnouncementNotificationServiceImpl&) = delete;
  AnnouncementNotificationServiceImpl& operator=(
      const AnnouncementNotificationServiceImpl&) = delete;

  ~AnnouncementNotificationServiceImpl() override = default;

 private:
  // AnnouncementNotificationService implementation.
  void MaybeShowNotification() override {
    // Finch config may not be delivered on first run. Records the first run
    // timestamp to check whether we want to skip the notification.
    RecordFirstRunIfNeeded();

    if (!IsFeatureEnabled())
      return;

    // No valid version Finch parameter.
    if (!IsVersionValid(remote_version_))
      return;

    // Update the version preference. This happens earilier than checks in
    // ShowNotification.
    int current_version = pref_service_->GetInteger(kCurrentVersionPrefName);
    pref_service_->SetInteger(kCurrentVersionPrefName, remote_version_);

    if (remote_version_ > current_version) {
      OnNewVersion();
    }
  }

  void RecordFirstRunIfNeeded() {
    if (!delegate_->IsFirstRun())
      return;

    pref_service_->SetTime(kAnnouncementFirstRunTimePrefName, clock_->Now());
  }

  bool IsFeatureEnabled() const {
    return base::FeatureList::IsEnabled(kAnnouncementNotification);
  }

  bool IsVersionValid(int version) const { return version >= 0; }

  void OnNewVersion() {
    if (ShouldSkipDueToFirstRun())
      return;

    if (skip_display_)
      return;

    // Skip new profile if needed.
    if (skip_new_profile_ && profile_->IsNewProfile())
      return;

    // Require signed out but the user signed in.
    if (require_signout_ && IsUserSignIn())
      return;

    // Check if we only want to show once.
    if (show_one_ && notification_shown)
      return;

    // Check profile type, some types can't create new navigation.
    if (!CanOpenAnnouncement(profile_))
      return;
    ShowNotification();
  }

  void ShowNotification() {
    notification_shown = true;
    delegate_->ShowNotification();
  }

  // Returns whether the notification should be skipped based on first run
  // controls defined in Finch.
  bool ShouldSkipDueToFirstRun() const {
    // Don't show notification for first run if Finch parameter specified
    // "skip_first_run" to true.
    if (delegate_->IsFirstRun() && skip_first_run_)
      return true;

    // Finch parameters is not guaranteed to receive by Chrome on first run.
    // Don't show notification if first run happens after the timestamp
    // specified in Finch parameter "skip_first_run_after_time".
    base::Time first_run_time =
        pref_service_->GetTime(kAnnouncementFirstRunTimePrefName);
    if (!first_run_time.is_null() && !skip_first_run_after_.is_null() &&
        first_run_time >= skip_first_run_after_) {
      return true;
    }

    // Notice that the first run can happen before
    // kAnnouncementFirstRunTimePrefName was added to the code base . In this
    // case, we do want to show the announcement.
    return false;
  }

  bool IsUserSignIn() {
    DCHECK(g_browser_process);
    // No browser process, assume the user is not signed in.
    if (!g_browser_process)
      return false;

    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();

    // Can't find the profile path, assume the user is not signed in.
    DCHECK(profile_);
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile_->GetPath());
    return entry && entry->GetSigninState() != SigninState::kNotSignedIn;
  }

  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  std::unique_ptr<Delegate> delegate_;
  raw_ptr<base::Clock> clock_;

  // Whether to skip first Chrome launch. Parsed from Finch.
  bool skip_first_run_;

  // Whether to skip new profile. Parsed from Finch.
  bool skip_new_profile_;

  // Don't show notification if true. Parsed from Finch.
  bool skip_display_;

  // The new notification version. Parsed from Finch.
  int remote_version_;

  // Whether only show notification to signed out users. Parsed from Finch.
  bool require_signout_;

  // Whether only show one notification in each Chrome launch. Parsed from
  // Finch.
  bool show_one_;

  // The announcement URL parsed from Finch. If empty then we use the default
  // URL.
  std::string remote_url_;

  // If first run happens after this time, then notification should not show.
  base::Time skip_first_run_after_;

  base::WeakPtrFactory<AnnouncementNotificationServiceImpl> weak_ptr_factory_{
      this};
};

BASE_FEATURE(kAnnouncementNotification,
             "AnnouncementNotificationService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
void AnnouncementNotificationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterIntegerPref(kCurrentVersionPrefName, kInvalidVersion);
  registry->RegisterTimePref(kAnnouncementFirstRunTimePrefName, base::Time());
}

// static
std::unique_ptr<AnnouncementNotificationService>
AnnouncementNotificationService::Create(Profile* profile,
                                        PrefService* pref_service,
                                        std::unique_ptr<Delegate> delegate,
                                        base::Clock* clock) {
  return std::make_unique<AnnouncementNotificationServiceImpl>(
      profile, pref_service, std::move(delegate), clock);
}

// static
GURL AnnouncementNotificationService::GetAnnouncementURL() {
  std::string remote_url = base::GetFieldTrialParamValueByFeature(
      kAnnouncementNotification, kAnnouncementUrl);
  // Fallback to default URL if |remote_url| is empty.
  std::string url = remote_url.empty()
                        ? l10n_util::GetStringUTF8(IDS_TOS_NOTIFICATION_LINK)
                        : remote_url;
  return GURL(url);
}

// static
bool AnnouncementNotificationService::CanOpenAnnouncement(Profile* profile) {
  DCHECK(profile);
  if (!profile)
    return false;

  return !profile->IsGuestSession() && !profile->IsSystemProfile();
}

AnnouncementNotificationService::AnnouncementNotificationService() = default;

AnnouncementNotificationService::~AnnouncementNotificationService() = default;
