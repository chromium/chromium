// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_ACTIONS_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_ACTIONS_H_

#include "base/functional/callback.h"

class BrowserWindowInterface;
class LocationBar;
class OmniboxPopupPresenterDelegate;

void RegisterOmniboxActions(
    base::RepeatingCallback<OmniboxPopupPresenterDelegate*(LocationBar*)>
        get_presenter_delegate,
    BrowserWindowInterface* browser);

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_ACTIONS_H_
