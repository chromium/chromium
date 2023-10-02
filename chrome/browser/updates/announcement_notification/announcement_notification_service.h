// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_

#include <memory>

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace base {
class Clock;
}  // namespace base

class PrefRegistrySimple;
class PrefService;
class Profile;

// Whether to enable announcement notification system.
BASE_DECLARE_FEATURE(kAnnouncementNotification);

// The Finch parameter name for a boolean value that whether to show
// notification on first run.
constexpr char kSkipFirstRun[] = "skip_first_run";

// The Finch parameter name for a string value that represents a time.
// If first run happens after this time, notification will not show.
// The string defined in Finch config should specify the time zone.
// e.g. 02 Feb 2020 13:00:00 GMT.
constexpr char kSkipFirstRunAfterTime[] = "skip_first_run_after_time";

// The Finch parameter name for a boolean value that whether to show
// notification for new profile.
constexpr char kSkipNewProfile[] = "skip_new_profile";

// The Finch parameter to control whether to skip notification display.
constexpr char kSkipDisplay[] = "skip_display";

// The Finch parameter to define the latest version number of the notification.
constexpr char kVersion[] = "version";

// The Finch parameter to define whether to show the announcement to users not
// signed in.
constexpr char kRequireSignout[] = "require_sign_out";

// The Finch parameter to define whether to show the announcement to users not
// signed in.
constexpr char kShowOneAllProfiles[] = "show_one_all_profiles";

// The Finch parameter to define the announcement URL.
constexpr char kAnnouncementUrl[] = "announcement_url";

// Preference name to persist the current version sent from Finch parameter.
constexpr char kCurrentVersionPrefName[] =
    "announcement_notification_service_current_version";

// Preference name to persist the time of Chrome first run.
constexpr char kAnnouncementFirstRunTimePrefName[] =
    "announcement_notification_service_first_run_time";

// Used to show a notification when the version defined in Finch parameter is
// higher than the last version saved in the preference service.
class AnnouncementNotificationService : public KeyedService {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Show notification.
    virtual void ShowNotification() = 0;

    // Is Chrome first time to run.
    virtual bool IsFirstRun() = 0;
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static std::unique_ptr<AnnouncementNotificationService> Create(
      Profile* profile,
      PrefService* pref_service,
      std::unique_ptr<Delegate> delegate,
      base::Clock* clock);
  static GURL GetAnnouncementURL();

  // Returns if the announcement can be opened.
  static bool CanOpenAnnouncement(Profile* profile);

  AnnouncementNotificationService();

  AnnouncementNotificationService(const AnnouncementNotificationService&) =
      delete;
  AnnouncementNotificationService& operator=(
      const AnnouncementNotificationService&) = delete;

  ~AnnouncementNotificationService() override;

  // Show notification if needed based on a version number in Finch parameters
  // and the version cached in PrefService.
  virtual void MaybeShowNotification() = 0;
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
