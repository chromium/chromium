// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import android.util.Pair;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.function.Function;

/** The screen that shows a loaded webpage with the omnibox and the toolbar. */
public class WebPageStation extends PageStation {
    public Element<WebContents> webContentsElement;
    public ViewElement<UrlBar> urlBarElement;

    protected <T extends WebPageStation> WebPageStation(Builder<T> builder) {
        super(builder);

        webContentsElement =
                declareEnterConditionAsElement(new WebContentsPresentCondition(loadedTabElement));
        declareEnterCondition(new FrameInfoUpdatedCondition(webContentsElement));

        // TODO(crbug.com/416558040): Do not add this if builder.mIgnoreUrlBar is set.
        // TODO(crbug.com/41497463): This should be shared, not unscoped, but the toolbar exists
        // in the tab switcher and it is not completely occluded.
        urlBarElement = declareView(URL_BAR, ViewElement.unscopedOption());

        // Make sure that the new tab page is not considered a WebPageStation
        List<String> prohibitedUrls = List.of("chrome://newtab", "chrome-native://newtab");
        declareEnterCondition(new PageUrlDoesNotMatchCondition(prohibitedUrls, loadedTabElement));
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

    /** Condition to check the page url does not match any of the prohibited urls. */
    public static class PageUrlDoesNotMatchCondition extends Condition {
        private final List<String> mProhibitedUrls;
        private final Supplier<Tab> mLoadedTabSupplier;

        public PageUrlDoesNotMatchCondition(
                List<String> prohibitedUrls, Supplier<Tab> loadedTabSupplier) {
            super(/* isRunOnUiThread= */ true);
            mProhibitedUrls = prohibitedUrls;
            mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            String url = mLoadedTabSupplier.get().getUrl().getSpec();
            for (String prohibitedUrl : mProhibitedUrls) {
                if (url.contains(prohibitedUrl)) {
                    return notFulfilled("URL: \"%s\"", url);
                }
            }
            return fulfilled("URL: \"%s\"", url);
        }

        @Override
        public String buildDescription() {
            return String.format("URL is not any of: %s", String.join(", ", mProhibitedUrls));
        }
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public RegularWebPageAppMenuFacility openRegularTabAppMenu() {
        assert !mIncognito;
        return enterFacilitySync(
                new RegularWebPageAppMenuFacility(), menuButtonElement.getClickTrigger());
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public IncognitoWebPageAppMenuFacility openIncognitoTabAppMenu() {
        assert mIncognito;
        return enterFacilitySync(
                new IncognitoWebPageAppMenuFacility(), menuButtonElement.getClickTrigger());
    }

    /** Trigger to scroll WebContents to the bottom. */
    public Transition.Trigger scrollToBottomTrigger() {
        return () -> {
            assertInPhase(Phase.ACTIVE);
            try {
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContentsElement.get(), "window.scrollTo(0, document.body.scrollHeight)");
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
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
        enterFacilitiesSync(
                List.of(omniboxFacility, softKeyboard), urlBarElement.getClickTrigger());
        return Pair.create(omniboxFacility, softKeyboard);
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
