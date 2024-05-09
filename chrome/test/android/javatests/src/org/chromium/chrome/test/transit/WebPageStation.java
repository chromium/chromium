// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.unscopedViewElement;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** The screen that shows a loaded webpage with the omnibox and the toolbar. */
public class WebPageStation extends PageStation {

    // TODO(crbug.com/41497463): This should be shared, not unscoped, but the toolbar exists in the
    // tab switcher and it is not completely occluded.
    public static final ViewElement URL_BAR = unscopedViewElement(withId(R.id.url_bar));

    protected Supplier<WebContents> mWebContentsSupplier;

    protected <T extends WebPageStation> WebPageStation(Builder<T> builder) {
        super(builder);
    }

    public static Builder<WebPageStation> newWebPageStationBuilder() {
        return new Builder<>(WebPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        mWebContentsSupplier =
                elements.declareEnterCondition(
                        new WebContentsPresentCondition(mPageLoadedCondition));

        elements.declareView(URL_BAR);
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public WebPageRegularAppMenuFacility openRegularTabAppMenu() {
        assert !mIncognito;

        WebPageRegularAppMenuFacility menu = new WebPageRegularAppMenuFacility(this);
        return Facility.enterSync(menu, () -> MENU_BUTTON.perform(click()));
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public WebPageIncognitoAppMenuFacility openIncognitoTabAppMenu() {
        assert mIncognito;

        WebPageIncognitoAppMenuFacility menu = new WebPageIncognitoAppMenuFacility(this);
        return Facility.enterSync(menu, () -> MENU_BUTTON.perform(click()));
    }

    private static class WebContentsPresentCondition extends InstrumentationThreadCondition
            implements Supplier<WebContents> {
        private final Supplier<Tab> mLoadedTabSupplier;

        public WebContentsPresentCondition(Supplier<Tab> loadedTabSupplier) {
            mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            return whether(hasValue());
        }

        @Override
        public String buildDescription() {
            return "WebContents present";
        }

        @Override
        public WebContents get() {
            Tab loadedTab = mLoadedTabSupplier.get();
            if (loadedTab == null) {
                return null;
            }
            return loadedTab.getWebContents();
        }

        @Override
        public boolean hasValue() {
            return get() != null;
        }
    }
}
