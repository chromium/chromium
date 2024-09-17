// Copyright 2020 The Chromium Authors
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

// Serves as a bridge between omnibox and other UIs. Allows registration of
// observers to listen for omnibox updates.
class OmniboxTabHelper : public content::WebContentsUserData<OmniboxTabHelper> {
 public:
  // Observer to listen for updates from OmniboxTabHelper.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the omnibox input state is changed.
    virtual void OnOmniboxInputStateChanged() = 0;

    // Invoked when the omnibox input is in progress.
    virtual void OnOmniboxInputInProgress(bool in_progress) = 0;

    // Called to indicate that the omnibox focus state changed with the given
    // |reason|.
    virtual void OnOmniboxFocusChanged(OmniboxFocusState state,
                                       OmniboxFocusChangeReason reason) = 0;

    // Invoked when the omnibox popup visibility changes.
    virtual void OnOmniboxPopupVisibilityChanged(bool popup_is_open) = 0;
  };

  ~OmniboxTabHelper() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Calls forwarded from omnibox. See OmniboxClient for details.
  void OnInputStateChanged();
  void OnInputInProgress(bool in_progress);
  void OnFocusChanged(OmniboxFocusState state, OmniboxFocusChangeReason reason);
  void OnPopupVisibilityChanged(bool popup_is_open);

 private:
  explicit OmniboxTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<OmniboxTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TAB_HELPER_H_
