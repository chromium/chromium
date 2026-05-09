// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.notifications;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.widget.TextView;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.transit.ui.BottomSheetFacility;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/**
 * Bottom Sheet main page that appears to access a Chrome tip when prompted from a notification.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class TipsPromoMainPageBottomSheetFacility<
                HostStationT extends Station<ChromeTabbedActivity>>
        extends BottomSheetFacility<HostStationT> {
    public final ViewElement<TextView> mainPageTitleElement;
    public final ViewElement<TextView> mainPageDescriptionElement;
    public final ViewElement<ButtonCompat> settingsButtonElement;
    public final ViewElement<ButtonCompat> detailsButtonElement;

    /** Constructor. */
    public TipsPromoMainPageBottomSheetFacility() {
        declareDescendantView(LottieAnimationView.class, withId(R.id.main_page_logo));
        mainPageTitleElement =
                declareDescendantView(TextView.class, withId(R.id.main_page_title_text));
        mainPageDescriptionElement =
                declareDescendantView(TextView.class, withId(R.id.main_page_description_text));
        settingsButtonElement =
                declareDescendantView(ButtonCompat.class, withId(R.id.tips_promo_settings_button));
        detailsButtonElement =
                declareDescendantView(ButtonCompat.class, withId(R.id.tips_promo_details_button));
    }

    /**
     * Press the details button to navigate to the details page.
     *
     * @param detailPageStepsRes The list of resource ids for the detail steps.
     */
    public TipsPromoDetailsPageBottomSheetFacility<HostStationT> clickDetailsButton(
            @Nullable List<Integer> detailPageStepsRes) {
        return detailsButtonElement
                .clickTo()
                .exitFacilityAnd()
                .enterFacility(new TipsPromoDetailsPageBottomSheetFacility<>(detailPageStepsRes));
    }

    /**
     * Press the details button to navigate to the details page without expectations for the detail
     * steps.
     */
    public TipsPromoDetailsPageBottomSheetFacility<HostStationT> clickDetailsButton() {
        return clickDetailsButton(null);
    }

    /** Performs the positive button click action for a given feature type. */
    @SuppressWarnings("unchecked")
    public <T> T clickPositiveButton(int featureType, Object... extraArgs) {
        return (T)
                NotificationsTestUtils.clickPositiveButton(
                        settingsButtonElement, featureType, mHostStation, extraArgs);
    }
}
