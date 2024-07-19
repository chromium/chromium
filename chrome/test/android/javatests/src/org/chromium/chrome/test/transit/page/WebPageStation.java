// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.unscopedViewElement;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;

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
                        new WebContentsPresentCondition(mPageLoadedSupplier));
        elements.declareEnterCondition(new FrameInfoUpdatedCondition(mWebContentsSupplier));

        elements.declareView(URL_BAR);
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public RegularWebPageAppMenuFacility openRegularTabAppMenu() {
        assert !mIncognito;
        return enterFacilitySync(
                new RegularWebPageAppMenuFacility(), () -> MENU_BUTTON.perform(click()));
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public IncognitoWebPageAppMenuFacility openIncognitoTabAppMenu() {
        assert mIncognito;
        return enterFacilitySync(
                new IncognitoWebPageAppMenuFacility(), () -> MENU_BUTTON.perform(click()));
    }

    private static class WebContentsPresentCondition extends ConditionWithResult<WebContents> {
        private final Supplier<Tab> mLoadedTabSupplier;

        public WebContentsPresentCondition(Supplier<Tab> loadedTabSupplier) {
            super(/* isRunOnUiThread= */ false);
            mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
        }

        @Override
        protected ConditionStatusWithResult<WebContents> resolveWithSuppliers() {
            return whether(hasValue()).withResult(get());
        }

        @Override
        public String buildDescription() {
            return "WebContents present";
        }

        @Override
        public WebContents get() {
            // Do not return a WebContents that has been destroyed, so always get it from the
            // Tab instead of letting ConditionWithResult return its |mResult|.
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

    private static class FrameInfoUpdatedCondition extends UiThreadCondition {
        Supplier<WebContents> mWebContentsSupplier;

        public FrameInfoUpdatedCondition(Supplier<WebContents> webContentsSupplier) {
            mWebContentsSupplier = dependOnSupplier(webContentsSupplier, "WebContents");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            Coordinates coordinates = Coordinates.createFor(mWebContentsSupplier.get());
            return whether(
                    coordinates.frameInfoUpdated(),
                    "frameInfoUpdated %b, pageScaleFactor: %.2f",
                    coordinates.frameInfoUpdated(),
                    coordinates.getPageScaleFactor());
        }

        @Override
        public String buildDescription() {
            return "WebContents frame info updated";
        }
    }
}
