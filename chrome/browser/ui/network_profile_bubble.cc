// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/network_profile_bubble.h"

#include <windows.h>

#include <stdint.h>
#include <wtsapi32.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// The duration of the silent period before we start nagging the user again.
const int kSilenceDurationDays = 100;

// The number of warnings to be shown on consequtive starts of Chrome before the
// silent period starts.
const int kMaxWarnings = 2;

// Implementation of BrowserListObserver used to wait for a browser
// window.
class NetworkProfileBubbleBrowserListObserver : public BrowserListObserver {
 private:
  ~NetworkProfileBubbleBrowserListObserver() override;

  // Overridden from ::BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;
};

NetworkProfileBubbleBrowserListObserver::
    ~NetworkProfileBubbleBrowserListObserver() {}

void NetworkProfileBubbleBrowserListObserver::OnBrowserAdded(Browser* browser) {
}

void NetworkProfileBubbleBrowserListObserver::OnBrowserRemoved(
    Browser* browser) {}

void NetworkProfileBubbleBrowserListObserver::OnBrowserSetLastActive(
    Browser* browser) {
  NetworkProfileBubble::ShowNotification(browser);
  // No need to observe anymore.
  BrowserList::RemoveObserver(this);
  delete this;
}

// The name of the UMA histogram collecting our stats.
const char kMetricNetworkedProfileCheck[] = "NetworkedProfile.Check";

}  // namespace

// static
bool NetworkProfileBubble::notification_shown_ = false;

// static
bool NetworkProfileBubble::ShouldCheckNetworkProfile(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetInteger(prefs::kNetworkProfileWarningsLeft))
    return !notification_shown_;
  int64_t last_check = prefs->GetInt64(prefs::kNetworkProfileLastWarningTime);
  base::TimeDelta time_since_last_check =
      base::Time::Now() - base::Time::FromTimeT(last_check);
  if (time_since_last_check.InDays() > kSilenceDurationDays) {
    prefs->SetInteger(prefs::kNetworkProfileWarningsLeft, kMaxWarnings);
    return !notification_shown_;
  }
  return false;
}

// static
void NetworkProfileBubble::CheckNetworkProfile(
    const base::FilePath& profile_folder) {
  // On Windows notify the users if their profiles are located on a network
  // share as we don't officially support this setup yet.
  // However we don't want to bother users on Cytrix setups as those have no
  // real choice and their admins must be well aware of the risks associated.
  // Also the command line flag --no-network-profile-warning can stop this
  // warning from popping up. In this case we can skip the check to make the
  // start faster.
  // Collect a lot of stats along the way to see which cases do occur in the
  // wild often enough.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoNetworkProfileWarning)) {
    RecordUmaEvent(METRIC_CHECK_SUPPRESSED);
    return;
  }

  LPWSTR buffer = NULL;
  DWORD buffer_length = 0;
  // Checking for RDP is cheaper than checking for a network drive so do this
  // one first.
  if (!::WTSQuerySessionInformation(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                    WTSClientProtocolType, &buffer,
                                    &buffer_length)) {
    RecordUmaEvent(METRIC_CHECK_FAILED);
    return;
  }

  unsigned short* type = reinterpret_cast<unsigned short*>(buffer);
  // We should warn the users if they have their profile on a network share only
  // if running on a local session.
  if (*type == WTS_PROTOCOL_TYPE_CONSOLE) {
    bool profile_on_network = false;
    if (!profile_folder.empty()) {
      base::FilePath temp_file;
      // Try to create some non-empty temp file in the profile dir and use
      // it to check if there is a reparse-point free path to it.
      if (base::CreateTemporaryFileInDir(profile_folder, &temp_file) &&
          base::WriteFile(temp_file, ".")) {
        base::FilePath normalized_temp_file;
        if (!base::NormalizeFilePath(temp_file, &normalized_temp_file))
          profile_on_network = true;
      } else {
        RecordUmaEvent(METRIC_CHECK_IO_FAILED);
      }
      base::DeleteFile(temp_file);
    }
    if (profile_on_network) {
      RecordUmaEvent(METRIC_PROFILE_ON_NETWORK);
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&NotifyNetworkProfileDetected));
    } else {
      RecordUmaEvent(METRIC_PROFILE_NOT_ON_NETWORK);
    }
  } else {
    RecordUmaEvent(METRIC_REMOTE_SESSION);
  }

  ::WTSFreeMemory(buffer);
}

// static
void NetworkProfileBubble::SetNotificationShown(bool shown) {
  notification_shown_ = shown;
}

// static
void NetworkProfileBubble::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNetworkProfileWarningsLeft,
                                kMaxWarnings);
  registry->RegisterInt64Pref(prefs::kNetworkProfileLastWarningTime, 0);
}

// static
void NetworkProfileBubble::RecordUmaEvent(MetricNetworkedProfileCheck event) {
  UMA_HISTOGRAM_ENUMERATION(kMetricNetworkedProfileCheck, event,
                            METRIC_NETWORKED_PROFILE_CHECK_SIZE);
}

// static
void NetworkProfileBubble::NotifyNetworkProfileDetected() {
  Browser* browser = chrome::FindLastActive();

  if (browser)
    ShowNotification(browser);
  else
    BrowserList::AddObserver(new NetworkProfileBubbleBrowserListObserver());
}
