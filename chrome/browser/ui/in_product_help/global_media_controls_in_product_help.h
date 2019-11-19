// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_GLOBAL_MEDIA_CONTROLS_IN_PRODUCT_HELP_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_GLOBAL_MEDIA_CONTROLS_IN_PRODUCT_HELP_H_

#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class Profile;

namespace feature_engagement {
class Tracker;
}

// Listens for the triggering conditions for the global media controls
// in-product help and starts the IPH flow at the appropriate time. This is a
// |Profile|-keyed service since we track interactions per user profile. Hooks
// throughout the browser UI code will fetch this service and notify it of
// interesting user actions.
class GlobalMediaControlsInProductHelp : public KeyedService,
                                         public TabStripModelObserver,
                                         public BrowserListObserver,
                                         public MediaToolbarButtonObserver {
 public:
  explicit GlobalMediaControlsInProductHelp(Profile* profile);
  ~GlobalMediaControlsInProductHelp() override;

  // TabStripModelObserver implementation.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserListObserver implementation.
  void OnBrowserClosing(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;

  // MediaToolbarButtonObserver implementation.
  void OnMediaDialogOpened() override;
  void OnMediaButtonShown() override {}
  void OnMediaButtonHidden() override;
  void OnMediaButtonEnabled() override;
  void OnMediaButtonDisabled() override;

  // Must be called when IPH promo finishes showing, whether by use of the
  // feature or by timing out.
  void HelpDismissed();

 private:
  // Called when we're pretty sure that a tab playing controllable media has
  // been backgrounded.
  void OnMediaBackgrounded();

  // Stops observing the tab strip for changes.
  void StopListening();

  feature_engagement::Tracker* GetTracker();

  Profile* const profile_;

  TabStripModel* observed_tab_strip_model_ = nullptr;

  // True if the currently focused tab is probably playing media.
  bool current_tab_likely_playing_media_ = false;

  DISALLOW_COPY_AND_ASSIGN(GlobalMediaControlsInProductHelp);
};

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_GLOBAL_MEDIA_CONTROLS_IN_PRODUCT_HELP_H_
