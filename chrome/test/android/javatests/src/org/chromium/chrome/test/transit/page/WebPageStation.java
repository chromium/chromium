// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import android.util.Pair;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.function.Function;

/** The screen that shows a loaded webpage with the omnibox and the toolbar. */
public class WebPageStation extends PageStation {
    protected Supplier<WebContents> mWebContentsSupplier;
    private boolean mIgnoreUrlBar;

    protected <T extends WebPageStation> WebPageStation(Builder<T> builder) {
        super(builder);
    }

    protected <T extends WebPageStation> WebPageStation(WebStationBuilder<T> builder) {
        super(builder);
        mIgnoreUrlBar = builder.mIgnoreUrlBar;
    }

    public static class WebStationBuilder<T extends WebPageStation> extends PageStation.Builder<T> {
        private boolean mIgnoreUrlBar;

        public WebStationBuilder(Function<PageStation.Builder<T>, T> factoryMethod) {
            super(factoryMethod);
        }

        /**
         * Set whether URL is a required element for this webpage station. This is used for pages
         * that doesn't show the URL bar (e.g. fullscreen page, or pages that scrolled off the
         * browser controls).
         */
        public WebStationBuilder<T> ignoreUrlBar(boolean ignoreUrlBar) {
            mIgnoreUrlBar = ignoreUrlBar;
            return this;
        }
    }

    public static WebStationBuilder<WebPageStation> newBuilder() {
        return new WebStationBuilder<>(WebPageStation::new);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);

        mWebContentsSupplier =
                elements.declareEnterCondition(
                        new WebContentsPresentCondition(mPageLoadedSupplier));
        elements.declareEnterCondition(new FrameInfoUpdatedCondition(mWebContentsSupplier));

        if (!mIgnoreUrlBar) {
            // TODO(crbug.com/41497463): This should be shared, not unscoped, but the toolbar exists
            // in the tab switcher and it is not completely occluded.
            elements.declareView(URL_BAR, ViewElement.unscopedOption());
        }
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public RegularWebPageAppMenuFacility openRegularTabAppMenu() {
        assert !mIncognito;
        return enterFacilitySync(new RegularWebPageAppMenuFacility(), MENU_BUTTON::click);
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public IncognitoWebPageAppMenuFacility openIncognitoTabAppMenu() {
        assert mIncognito;
        return enterFacilitySync(new IncognitoWebPageAppMenuFacility(), MENU_BUTTON::click);
    }

    /** Trigger to scroll WebContents to the bottom. */
    public Transition.Trigger scrollToBottomTrigger() {
        return () -> {
            assertSuppliersCanBeUsed();
            try {
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mWebContentsSupplier.get(),
                        "window.scrollTo(0, document.body.scrollHeight)");
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
        };
    }

    /** Click the URL bar to enter the Omnibox. */
    public Pair<OmniboxFacility, SoftKeyboardFacility> openOmnibox(
            FakeOmniboxSuggestions fakeSuggestions) {
        OmniboxFacility omniboxFacility =
                new OmniboxFacility(/* incognito= */ mIncognito, fakeSuggestions);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility(mActivityElement);
        enterFacilitiesSync(List.of(omniboxFacility, softKeyboard), URL_BAR::click);
        return Pair.create(omniboxFacility, softKeyboard);
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

    // Condition checks whether web page reaches the bottom by checking viewport position and scroll
    // elements heights.
    protected static class ScrollToBottomCondition extends Condition {
        Supplier<WebContents> mWebContentsSupplier;

        public ScrollToBottomCondition(Supplier<WebContents> webContentsSupplier) {
            super(/* isRunOnUiThread= */ false);
            mWebContentsSupplier = webContentsSupplier;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            if (isPageScrolledToBottom(mWebContentsSupplier.get())) {
                return fulfilled();
            }
            return notFulfilled("Not scrolled to the bottom yet.");
        }

        @Override
        public String buildDescription() {
            return "Page scrolled to the bottom.";
        }

        private static boolean isPageScrolledToBottom(WebContents wc) {
            String code =
                    "window.visualViewport.pageTop + window.visualViewport.height "
                            + ">= document.scrollingElement.scrollHeight - 1";
            try {
                return Boolean.parseBoolean(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(wc, code));
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
        }
    }
}
