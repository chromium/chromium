// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_LIVE_CAPTION_IN_PRODUCT_HELP_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_LIVE_CAPTION_IN_PRODUCT_HELP_H_

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace feature_engagement {
class Tracker;
}

// Listens for the triggering conditions for the global media controls
// in-product help and starts the IPH flow at the appropriate time. This is a
// |Profile|-keyed service since we track interactions per user profile. Hooks
// throughout the browser UI code will fetch this service and notify it of
// interesting user actions.
class LiveCaptionInProductHelp : public KeyedService,
                                 public MediaToolbarButtonObserver {
 public:
  explicit LiveCaptionInProductHelp(Profile* profile);
  ~LiveCaptionInProductHelp() override;
  LiveCaptionInProductHelp(const LiveCaptionInProductHelp&) = delete;
  LiveCaptionInProductHelp& operator=(const LiveCaptionInProductHelp&) = delete;

  // MediaToolbarButtonObserver:
  void OnMediaDialogOpened() override {}
  void OnMediaButtonShown() override {}
  void OnMediaButtonHidden() override {}
  void OnMediaButtonEnabled() override;
  void OnMediaButtonDisabled() override {}

  // Must be called when IPH promo finishes showing, whether by use of the
  // feature or by timing out.
  void HelpDismissed();

 private:
  feature_engagement::Tracker* GetTracker();
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_LIVE_CAPTION_IN_PRODUCT_HELP_H_
