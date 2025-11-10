// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SERVICE_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class ProfileBrowserCollection;

// This per-profile class holds profile-scoped state for the read anything
// feature.
class ReadAnythingService : public KeyedService,
                            public BrowserCollectionObserver {
 public:
  explicit ReadAnythingService(Profile* profile);
  ~ReadAnythingService() override;

  static ReadAnythingService* Get(Profile* profile);
#if !BUILDFLAG(IS_CHROMEOS)
  static void InstallComponent(const base::FilePath& new_dir);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Called by the per-tab ReadAnythingSidePanelController.
  void OnReadAnythingSidePanelEntryShown();
  void OnReadAnythingSidePanelEntryHidden();

 private:
  void InstallGDocsHelperExtension();
  void RemoveGDocsHelperExtension();
  void RemoveTtsDownloadExtension();
  void OnLocalSidePanelSwitchDelayTimeout();
  static void RecordEngineVersion(const base::FilePath& engine_version);
#if !BUILDFLAG(IS_CHROMEOS)
  void SetupDesktopEngine();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

  // The number of active local side panels that are currently shown. If there
  // is no active local side panel (count is 0) after a timeout, we can safely
  // remove the gdocs helper extension.
  int active_local_side_panel_count_ = 0;

  // Start a timer when the user leaves a local side panel. If they switch to
  // another local side panel before it expires, keep the extension installed;
  // otherwise, uninstall it. This prevents frequent
  // installations/uninstallations.
  base::RetainingOneShotTimer local_side_panel_switch_delay_timer_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observer_{this};

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<ReadAnythingService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SERVICE_H_
