// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_INFOBAR_OBSERVER_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_INFOBAR_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserWindowInterface;
class Profile;
class TabStripModel;

// This class is responsible for running a callback that adds infobar on the
// the next browser created with the provided Profile.
class StartupInfoBarObserver : public BrowserCollectionObserver,
                               public TabStripModelObserver,
                               public ProfileObserver,
                               public base::SupportsUserData::Data {
 public:
  static constexpr char kStartupInfoBarObserverKey[] =
      "startup-infobar-observer";

  using AddInfoBarsCallback = base::OnceCallback<void(BrowserWindowInterface*)>;

  ~StartupInfoBarObserver() override;

  StartupInfoBarObserver(const StartupInfoBarObserver&) = delete;
  StartupInfoBarObserver& operator=(const StartupInfoBarObserver&) = delete;

  // Factory method for checking if the profile if eligible, creating the
  // observer and adding to Profile UserData.
  static void ObserveProfile(Profile& profile, AddInfoBarsCallback callback);

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  StartupInfoBarObserver(Profile& profile,
                         ProfileBrowserCollection& profile_browser_collection,
                         AddInfoBarsCallback callback);

  void AddInfoBarsAndReset();
  void Reset();

  const raw_ref<Profile> profile_;
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;

  AddInfoBarsCallback add_infobars_callback_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_INFOBAR_OBSERVER_H_
