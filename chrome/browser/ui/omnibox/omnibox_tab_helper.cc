// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"

#include "base/observer_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/omnibox/browser/omnibox_view.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(OmniboxTabHelper);

OmniboxTabHelper::~OmniboxTabHelper() = default;
OmniboxTabHelper::OmniboxTabHelper(content::WebContents* contents)
    : content::WebContentsUserData<OmniboxTabHelper>(*contents) {}

void OmniboxTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxTabHelper::OnInputStateChanged() {
  for (auto& observer : observers_) {
    observer.OnOmniboxInputStateChanged();
  }
}

void OmniboxTabHelper::OnInputInProgress(bool in_progress) {
  for (auto& observer : observers_) {
    observer.OnOmniboxInputInProgress(in_progress);
  }
}

void OmniboxTabHelper::OnFocusChanged(OmniboxFocusState state,
                                      OmniboxFocusChangeReason reason) {
  for (auto& observer : observers_) {
    observer.OnOmniboxFocusChanged(state, reason);
  }
}

void OmniboxTabHelper::OnPopupVisibilityChanged(bool popup_is_open) {
  for (auto& observer : observers_) {
    observer.OnOmniboxPopupVisibilityChanged(popup_is_open);
  }
}
