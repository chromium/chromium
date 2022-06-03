// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TAB_HELPER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TAB_HELPER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Serves as a bridge between omnibox and NTP UIs. Allows registration of
// observers to listen for omnibox updates.
class OmniboxTabHelper : public content::WebContentsUserData<OmniboxTabHelper> {
 public:
  // Observer to listen for updates from OmniboxTabHelper.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the omnibox input state is changed.
    virtual void OnOmniboxInputStateChanged() = 0;
    // Called to indicate that the omnibox focus state changed with the given
    // |reason|.
    virtual void OnOmniboxFocusChanged(OmniboxFocusState state,
                                       OmniboxFocusChangeReason reason) = 0;
  };

  ~OmniboxTabHelper() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Calls forwarded from omnibox. See OmniboxClient for details.
  void OnInputStateChanged();
  void OnFocusChanged(OmniboxFocusState state, OmniboxFocusChangeReason reason);

 private:
  explicit OmniboxTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<OmniboxTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TAB_HELPER_H_
