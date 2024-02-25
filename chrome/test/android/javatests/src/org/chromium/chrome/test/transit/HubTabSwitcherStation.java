// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Regular tab switcher pane station. */
public class HubTabSwitcherStation extends HubTabSwitcherBaseStation {
    /**
     * @param chromeTabbedActivityTestRule The activity rule under test.
     */
    public HubTabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        super(chromeTabbedActivityTestRule);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }
}
