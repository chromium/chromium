// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import org.chromium.chrome.browser.hub.PaneId;

import java.util.Map;

/** Utilities for handling Hub related stations. */
public class HubStationUtils {
    // The non-incognito contentDescription is a substring found in the following string:
    // R.string.accessibility_tab_switcher_standard_stack.
    // The incognito contentDescription is the actual string found in the resource:
    // R.string.accessibility_tab_switcher_incognito_stack.
    private static Map<Integer, String> sPaneIdToContentDescriptionMap =
            Map.ofEntries(
                    Map.entry(PaneId.TAB_SWITCHER, "standard tab"),
                    Map.entry(PaneId.INCOGNITO_TAB_SWITCHER, "Incognito tabs"));

    /**
     * @param paneId The pane to get the content description of.
     * @return the string resource ID for the content description used to select the pane.
     */
    public static String getContentDescriptionSubstringForIdPaneSelection(@PaneId int paneId) {
        String contentDescription = sPaneIdToContentDescriptionMap.get(paneId);
        if (contentDescription == null) {
            throw new IllegalArgumentException("No pane registered for " + paneId);
        }
        return contentDescription;
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
