// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/profile_launch_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "content/public/browser/browser_thread.h"

ProfileLaunchObserver::ProfileLaunchObserver() {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
}

ProfileLaunchObserver::~ProfileLaunchObserver() = default;

// static
ProfileLaunchObserver* ProfileLaunchObserver::GetInstance() {
  return g_browser_process->GetFeatures()->profile_launch_observer();
}

// static
void ProfileLaunchObserver::AddLaunched(Profile* profile) {
  GetInstance()->AddLaunchedInternal(profile);
}

// static
bool ProfileLaunchObserver::HasBeenLaunchedAndBrowserOpen(
    const Profile* profile) {
  return GetInstance()->HasBeenLaunchedAndBrowserOpenInternal(profile);
}

// static
void ProfileLaunchObserver::ClearForTesting() {
  GetInstance()->Clear();
}

// static
void ProfileLaunchObserver::set_profile_to_activate(Profile* profile) {
  GetInstance()->set_profile_to_activate_internal(profile);
}

// static
bool ProfileLaunchObserver::activated_profile() {
  return GetInstance()->activated_profile_internal();
}

void ProfileLaunchObserver::OnBrowserCreated(BrowserWindowInterface* browser) {
  opened_profiles_.insert(browser->GetProfile());
  MaybeActivateProfile();
}

void ProfileLaunchObserver::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  launched_profiles_.erase(profile);
  opened_profiles_.erase(profile);
  if (profile == profile_to_activate_) {
    profile_to_activate_ = nullptr;
  }
  // If this profile was the last launched one without an opened window,
  // then we may be ready to activate |profile_to_activate_|.
  MaybeActivateProfile();
}

bool ProfileLaunchObserver::HasBeenLaunchedAndBrowserOpenInternal(
    const Profile* profile) const {
  return opened_profiles_.contains(profile) &&
         launched_profiles_.contains(profile);
}

void ProfileLaunchObserver::AddLaunchedInternal(Profile* profile) {
  if (!observed_profiles_.IsObservingSource(profile)) {
    observed_profiles_.AddObservation(profile);
  }
  launched_profiles_.insert(profile);
  if (chrome::FindBrowserWithProfile(profile)) {
    // A browser may get opened before we get initialized (e.g., in tests),
    // so we never see the OnBrowserAdded() for it.
    opened_profiles_.insert(profile);
  }
}

void ProfileLaunchObserver::Clear() {
  launched_profiles_.clear();
  opened_profiles_.clear();
}

bool ProfileLaunchObserver::activated_profile_internal() {
  return activated_profile_;
}

void ProfileLaunchObserver::set_profile_to_activate_internal(Profile* profile) {
  if (!observed_profiles_.IsObservingSource(profile)) {
    observed_profiles_.AddObservation(profile);
  }
  profile_to_activate_ = profile;
  MaybeActivateProfile();
}

void ProfileLaunchObserver::MaybeActivateProfile() {
  if (!profile_to_activate_) {
    return;
  }
  // Check that browsers have been opened for all the launched profiles.
  // Note that browsers opened for profiles that were not added as launched
  // profiles are simply ignored.
  auto i = launched_profiles_.begin();
  for (; i != launched_profiles_.end(); ++i) {
    if (opened_profiles_.find(*i) == opened_profiles_.end()) {
      return;
    }
  }
  // Asynchronous post to give a chance to the last window to completely
  // open and activate before trying to activate |profile_to_activate_|.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ProfileLaunchObserver::ActivateProfile,
                                base::Unretained(this)));
  // Avoid posting more than once before ActivateProfile gets called.
  observed_profiles_.RemoveAllObservations();
  browser_collection_observation_.Reset();
}

void ProfileLaunchObserver::ActivateProfile() {
  // We need to test again, in case the profile got deleted in the mean time.
  if (profile_to_activate_) {
    Browser* browser = chrome::FindBrowserWithProfile(profile_to_activate_);
    // |profile| may never get launched, e.g., if it only had
    // incognito Windows and one of them was used to exit Chrome.
    // So it won't have a browser in that case.
    if (browser) {
      browser->window()->Activate();
    }
    // No need try to activate this profile again.
    profile_to_activate_ = nullptr;
  }
  // Assign true here, even if no browser was actually activated, so that
  // the test can stop waiting, and fail gracefully when needed.
  activated_profile_ = true;
}
