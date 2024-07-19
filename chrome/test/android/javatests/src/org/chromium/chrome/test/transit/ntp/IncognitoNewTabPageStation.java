// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.ntp;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import android.content.res.Configuration;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.transit.page.PageStation;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends PageStation {
    public ViewElement ICON = scopedViewElement(withId(R.id.new_tab_incognito_icon));
    public ViewElement GONE_INCOGNITO_TEXT = scopedViewElement(withText("You’ve gone Incognito"));

    protected <T extends IncognitoNewTabPageStation> IncognitoNewTabPageStation(
            Builder<T> builder) {
        super(builder.withIncognito(true));
    }

    public static Builder<IncognitoNewTabPageStation> newBuilder() {
        return new Builder<>(IncognitoNewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        elements.declareView(ICON);
        elements.declareElementFactory(
                mActivityElement.getEnterCondition(),
                delayedElements -> {
                    // TODO(crbug.com/351378295): In tablets with a hardware keyboard connected, the
                    // omnibox gets focus automatically. By default this doesn't open the soft
                    // keyboard, but the soft keyboard might open if the setting
                    // "Physical keyboard" > "Show on-screen keyboard" is on, which is the default
                    // in emulators.
                    //
                    // In landscape mode, the soft keyboard occludes this text.
                    if (!isTabletInLandscape(mActivityElement.get())) {
                        delayedElements.declareView(GONE_INCOGNITO_TEXT);
                    }
                });
        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedSupplier));
    }

    private static boolean isTabletInLandscape(ChromeTabbedActivity activity) {
        return activity.isTablet()
                && activity.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE;
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public IncognitoNewTabPageAppMenuFacility openAppMenu() {
        return enterFacilitySync(
                new IncognitoNewTabPageAppMenuFacility(), () -> MENU_BUTTON.perform(click()));
    }
}
