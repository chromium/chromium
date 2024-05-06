// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_STORAGE_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_STORAGE_H_

#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace privacy_sandbox {

// Different notice actions stored in the pref above.
enum class NoticeActionTaken {
  // No Ack action set.
  kNotSet = 0,
  // Action taken some other way.
  kOther = 1,
  // ACK'ed the notice using 'GotIt' or some other form of acknowledgement.
  kAck = 2,
  // Opted in/Consented to the notice using 'Turn it on' or some other form of
  // explicit consent.
  kOptIn = 3,
  // Action taken to dismiss or opt out of the notice using 'No Thanks' or some
  // other form of dismissal.
  kOptOut = 4,
  // Action taken clicking the settings button.
  kSettings = 5,
  // Action taken clicking the learn more button.
  kLearnMore = 6,
  // Action taken clicking the 'x' button, closing the browser etc.
  kClosed = 7,
  kMaxValue = kClosed,
};

// Stores information about profile interactions on a notice.
struct PrivacySandboxNoticeData {
  PrivacySandboxNoticeData();
  PrivacySandboxNoticeData& operator=(const PrivacySandboxNoticeData&);
  ~PrivacySandboxNoticeData();
  PrivacySandboxNoticeData(int schema_version,
                           NoticeActionTaken notice_action_taken,
                           base::Time notice_action_taken_time,
                           base::Time notice_first_shown,
                           base::Time notice_last_shown,
                           base::TimeDelta notice_shown_duration);
  int schema_version = 0;
  NoticeActionTaken notice_action_taken = NoticeActionTaken::kNotSet;
  base::Time notice_action_taken_time;
  base::Time notice_first_shown;
  base::Time notice_last_shown;
  base::TimeDelta notice_shown_duration;
};

class PrivacySandboxNoticeStorage {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  std::optional<PrivacySandboxNoticeData> ReadNoticeData(
      PrefService* pref_service,
      std::string_view notice);

  void SetSchemaVersion(PrefService* pref_service,
                        std::string_view notice,
                        int schema_version);
  void SetNoticeActionTaken(PrefService* pref_service,
                            std::string_view notice,
                            NoticeActionTaken notice_action_taken,
                            base::Time notice_action_taken_time);
  void SetNoticeShown(PrefService* pref_service,
                      std::string_view notice,
                      base::Time notice_shown_time);
  void SetNoticeShownDuration(PrefService* pref_service,
                              std::string_view notice,
                              base::TimeDelta notice_shown_duration);

  PrivacySandboxNoticeStorage(const PrivacySandboxNoticeStorage&) = delete;
  PrivacySandboxNoticeStorage& operator=(const PrivacySandboxNoticeStorage&) =
      delete;

 private:
  friend class PrivacySandboxNoticeStorageTestPeer;  // For testing.
  friend base::NoDestructor<PrivacySandboxNoticeStorage>;
  PrivacySandboxNoticeStorage() = default;
  ~PrivacySandboxNoticeStorage() = default;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_STORAGE_H_
