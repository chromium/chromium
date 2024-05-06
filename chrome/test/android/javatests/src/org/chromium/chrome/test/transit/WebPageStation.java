// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.unscopedViewElement;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.transit.WebContentsElementInState;

/** The screen that shows a loaded webpage with the omnibox and the toolbar. */
public class WebPageStation extends PageStation {

    // TODO(crbug.com/41497463): This should be shared, not unscoped, but the toolbar exists in the
    // tab switcher and it is not completely occluded.
    public static final ViewElement URL_BAR = unscopedViewElement(withId(R.id.url_bar));

    protected WebContentsElementInState mWebContents;

    protected <T extends WebPageStation> WebPageStation(Builder<T> builder) {
        super(builder);
    }

    public static Builder<WebPageStation> newWebPageStationBuilder() {
        return new Builder<>(WebPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        elements.declareView(URL_BAR);
        mWebContents =
                elements.declareElementInState(
                        new WebContentsElementInState(
                                () -> {
                                    ChromeTabbedActivity activity = mActivityElement.get();
                                    if (activity == null) {
                                        return null;
                                    }
                                    Tab activityTab = activity.getActivityTab();
                                    if (activityTab == null) {
                                        return null;
                                    }
                                    return activityTab.getWebContents();
                                }));
    }
}
