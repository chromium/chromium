// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.hub.PaneId;

import java.util.Map;

/** Utilities for handling Hub related stations. */
public class HubStationUtils {
    private static Map<Integer, Integer> sPaneIdToContentDescriptionIdMap =
            Map.ofEntries(
                    Map.entry(
                            PaneId.TAB_SWITCHER,
                            R.string.accessibility_tab_switcher_standard_stack),
                    Map.entry(
                            PaneId.INCOGNITO_TAB_SWITCHER,
                            R.string.accessibility_tab_switcher_incognito_stack));

    /**
     * @param paneId The pane to get the content description of.
     * @return the string resource ID for the content description used to select the pane.
     */
    public static @StringRes int getContentDescriptionForIdPaneSelection(@PaneId int paneId) {
        Integer contentDescription = sPaneIdToContentDescriptionIdMap.get(paneId);
        if (contentDescription == null) {
            throw new IllegalArgumentException("No pane registered for " + paneId);
        }
        return contentDescription.intValue();
    }

    /**
     * @param paneId The pane to create the station for.
     * @return corresponding {@link HubBaseStation} subclass.
     */
    public static HubBaseStation createHubStation(
            @PaneId int paneId, boolean regularTabsExist, boolean incognitoTabsExist) {
        switch (paneId) {
            case PaneId.TAB_SWITCHER:
                return new RegularTabSwitcherStation(regularTabsExist, incognitoTabsExist);
            case PaneId.INCOGNITO_TAB_SWITCHER:
                return new IncognitoTabSwitcherStation(regularTabsExist, incognitoTabsExist);
            default:
                throw new IllegalArgumentException("No hub station is available for " + paneId);
        }
    }
}
