// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.isSelected;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.hub.PaneId;

/** Incognito tab switcher pane station. */
public class IncognitoTabSwitcherStation extends TabSwitcherStation {

    public static final ViewElement SELECTED_INCOGNITO_TOGGLE_TAB_BUTTON =
            scopedViewElement(allOf(INCOGNITO_TOGGLE_TAB_BUTTON.getViewMatcher(), isSelected()));

    public IncognitoTabSwitcherStation(boolean regularTabsExist, boolean incognitoTabsExist) {
        super(/* isIncognito= */ true, regularTabsExist, incognitoTabsExist);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        elements.declareView(SELECTED_INCOGNITO_TOGGLE_TAB_BUTTON);
    }
}
