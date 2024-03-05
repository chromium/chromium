// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Incognito tab switcher pane station. */
public class HubIncognitoTabSwitcherStation extends HubTabSwitcherBaseStation {
    /**
     * @param chromeTabbedActivityTestRule The activity rule under test.
     */
    public HubIncognitoTabSwitcherStation(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(chromeTabbedActivityTestRule, /* isIncognito= */ true);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }
}
