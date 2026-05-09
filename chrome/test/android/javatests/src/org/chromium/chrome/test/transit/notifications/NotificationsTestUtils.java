// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.notifications;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.transit.signin.SigninBottomSheetFacility;

/** Utility class for Tips Notifications tests. */
public class NotificationsTestUtils {
    /** Performs the positive button click action for a given feature type. */
    public static <HostStationT extends Station<ChromeTabbedActivity>> Object clickPositiveButton(
            ViewElement<?> settingsButtonElement,
            int featureType,
            HostStationT hostStation,
            Object... extraArgs) {
        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                return settingsButtonElement
                        .clickTo()
                        .exitFacilityAnd()
                        .arriveAt(new SettingsStation<>(SafeBrowsingSettingsFragment.class));
            case TipsNotificationsFeatureType.QUICK_DELETE:
                return settingsButtonElement
                        .clickTo()
                        .exitFacilityAnd()
                        .enterFacility(new QuickDeleteDialogFacility());
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                LensController lensController = (LensController) extraArgs[0];
                ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
                settingsButtonElement
                        .clickTo()
                        .waitForAnd(new LensIntentFulfilledCondition(lensController))
                        .exitFacility();
                return null;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                return settingsButtonElement
                        .clickTo()
                        .exitFacilityAnd()
                        .arriveAt(new SettingsStation<>(AddressBarSettingsFragment.class));
            case TipsNotificationsFeatureType.PASSWORD_AUTOFILL:
            case TipsNotificationsFeatureType.CUSTOMIZE_MVT:
                settingsButtonElement.clickTo().exitFacility();
                return null;
            case TipsNotificationsFeatureType.SIGNIN:
                return settingsButtonElement
                        .clickTo()
                        .exitFacilityAnd()
                        .enterFacility(new SigninBottomSheetFacility<>());
            case TipsNotificationsFeatureType.CREATE_TAB_GROUPS:
                return settingsButtonElement
                        .clickTo()
                        .exitFacilityAnd()
                        .arriveAt(
                                RegularTabSwitcherStation.from(
                                        hostStation.getActivity().getTabModelSelector()));
            case TipsNotificationsFeatureType.RECENT_TABS:
                return settingsButtonElement
                        .clickTo()
                        .exitFacilityAnd()
                        .arriveAt(CtaPageStation.newGenericBuilder().initOpeningNewTab().build());
            default:
                throw new IllegalArgumentException("Unknown feature type: " + featureType);
        }
    }
}
