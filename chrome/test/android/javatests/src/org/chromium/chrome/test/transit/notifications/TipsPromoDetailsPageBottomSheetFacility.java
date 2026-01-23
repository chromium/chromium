// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.notifications;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import android.widget.ImageButton;
import android.widget.TextView;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.transit.ui.BottomSheetFacility;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/**
 * Bottom Sheet details page that appears to access a Chrome tip when prompted from a notification.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class TipsPromoDetailsPageBottomSheetFacility<
                HostStationT extends Station<ChromeTabbedActivity>>
        extends BottomSheetFacility<HostStationT> {
    public final ViewElement<TextView> detailPageTitleElement;
    public final ViewElement<ImageButton> backButtonElement;
    public final ViewElement<ButtonCompat> settingsButtonElement;

    /** Constructor. Take a list of detail steps resource ids. */
    public TipsPromoDetailsPageBottomSheetFacility(@Nullable List<Integer> detailPageStepsRes) {
        detailPageTitleElement =
                declareDescendantView(TextView.class, withId(R.id.details_page_title_text));
        backButtonElement =
                declareDescendantView(ImageButton.class, withId(R.id.details_page_back_button));
        settingsButtonElement =
                declareDescendantView(
                        ButtonCompat.class, withId(R.id.tips_promo_details_settings_button));

        if (detailPageStepsRes != null) {
            for (int i = 0; i < detailPageStepsRes.size(); i++) {
                int step = detailPageStepsRes.get(i);
                declareDescendantView(
                        allOf(withId(R.id.step_number), withText(String.valueOf(i + 1))));
                declareDescendantView(allOf(withId(R.id.step_content), withText(step)));
            }
        }
    }

    /** Press the back button to navigate back to the main page. */
    public TipsPromoMainPageBottomSheetFacility clickBackButton() {
        return backButtonElement
                .clickTo()
                .exitFacilityAnd()
                .enterFacility(new TipsPromoMainPageBottomSheetFacility<>());
    }

    /** Use the system backpress to navigate back to the main page. */
    public TipsPromoMainPageBottomSheetFacility pressBack() {
        recheckActiveConditions();
        return pressBackTo()
                .exitFacilityAnd()
                .enterFacility(new TipsPromoMainPageBottomSheetFacility<>());
    }

    /** Press the settings button to navigate to the safe browsing settings page. */
    public SettingsStation<SafeBrowsingSettingsFragment> clickESBSettingsButton() {
        return settingsButtonElement
                .clickTo()
                .exitFacilityAnd()
                .arriveAt(new SettingsStation<>(SafeBrowsingSettingsFragment.class));
    }

    /** Press the settings button to navigate to the quick delete page. */
    public QuickDeleteDialogFacility clickQuickDeleteButton() {
        return settingsButtonElement
                .clickTo()
                .exitFacilityAnd()
                .enterFacility(new QuickDeleteDialogFacility());
    }

    /** Press the settings button to navigate to Google Lens. */
    public void clickGoogleLensButton(LensController lensController) {
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
        settingsButtonElement
                .clickTo()
                .waitForAnd(new LensIntentFulfilledCondition(lensController))
                .exitFacility();
    }

    /** Press the settings button to navigate to the bottom omnibox settings page. */
    public SettingsStation<AddressBarSettingsFragment> clickBottomOmniboxSettingsButton() {
        return settingsButtonElement
                .clickTo()
                .exitFacilityAnd()
                .arriveAt(new SettingsStation<>(AddressBarSettingsFragment.class));
    }
}
