// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** The Incognito New Tab Page screen, with text about Incognito mode. */
public class IncognitoNewTabPageStation extends PageStation {
    public ViewElement ICON = sharedViewElement(withId(R.id.new_tab_incognito_icon));
    public ViewElement GONE_INCOGNITO_TEXT = sharedViewElement(withText("You’ve gone Incognito"));
    public ViewElement REVAMPED_ICON = sharedViewElement(withId(R.id.revamped_incognito_ntp_icon));
    public ViewElement REVAMPED_DOES_TEXT = sharedViewElement(withText("What Incognito does"));
    public ViewElement REVAMPED_DOESNT_TEXT =
            sharedViewElement(withText("What Incognito doesn’t do"));

    protected IncognitoNewTabPageStation(Builder<IncognitoNewTabPageStation> builder) {
        super(builder.withIncognito(true));
    }

    public static Builder<IncognitoNewTabPageStation> newBuilder() {
        return new Builder<>(IncognitoNewTabPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        boolean isTablet = mChromeTabbedActivityTestRule.getActivity().isTablet();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_NTP_REVAMP)) {
            elements.declareView(REVAMPED_ICON);
            elements.declareView(REVAMPED_DOES_TEXT);
            // TODO(crbug.com/335440649): On generic_android32_foldable the soft keyboard is
            // opened over this text.
            if (!isTablet) {
                elements.declareView(REVAMPED_DOESNT_TEXT);
            }
        } else {
            elements.declareView(ICON);
            elements.declareView(GONE_INCOGNITO_TEXT);
        }

        elements.declareEnterCondition(new NtpLoadedCondition(mPageLoadedEnterCondition));
    }
}
