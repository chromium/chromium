// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import android.util.Pair;
import android.view.View;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/** The screen that shows a loaded webpage with the omnibox and the toolbar. */
public class WebPageStation extends CtaPageStation {
    public Element<WebContents> webContentsElement;
    public ViewElement<UrlBar> urlBarElement;

    protected WebPageStation(Config config) {
        super(config);

        webContentsElement =
                declareEnterConditionAsElement(new WebContentsPresentCondition(loadedTabElement));
        declareEnterCondition(new FrameInfoUpdatedCondition(webContentsElement));

        // TODO(crbug.com/41497463): This should be shared, not unscoped, but the toolbar exists
        // in the tab switcher and it is not completely occluded.
        urlBarElement = declareView(URL_BAR, ViewElement.unscopedOption());

        // Make sure that the new tab page is not considered a WebPageStation
        List<String> prohibitedUrls = List.of("chrome://newtab", "chrome-native://newtab");
        declareEnterCondition(new PageUrlDoesNotMatchCondition(prohibitedUrls, loadedTabElement));
    }

    public static Builder<WebPageStation> newBuilder() {
        return new Builder<>(WebPageStation::new);
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
                    return notFulfilled(
                            "URL is \"%s\", which is prohibited. Use the appropriate Station"
                                    + " subclass instead of WebPageStation.",
                            url);
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
        assert !mIsIncognito;
        return menuButtonElement.clickTo().enterFacility(new RegularWebPageAppMenuFacility());
    }

    /** Opens the web page app menu by pressing the toolbar "..." button */
    public IncognitoWebPageAppMenuFacility openIncognitoTabAppMenu() {
        assert mIsIncognito;
        return menuButtonElement.clickTo().enterFacility(new IncognitoWebPageAppMenuFacility());
    }

    /** Scrolls down the page using a drag gesture to dismiss browser controls. */
    public TripBuilder scrollPageDownWithGestureTo() {
        return runTo(
                () -> {
                    assertInPhase(Phase.ACTIVE);
                    View contentView = activityTabElement.value().getView();
                    float width = contentView.getWidth();
                    float height = contentView.getHeight();
                    // Start the scroll with some height to avoid touching the nav bar region.
                    float fromY = height - height / 10;
                    float toY = 0;
                    TouchCommon.performDragNoFling(
                            mActivityElement.value(),
                            width / 2,
                            width / 2,
                            fromY,
                            toY,
                            /* stepCount= */ 50,
                            /* duration= */ 500);
                });
    }

    /** Scrolls up the page using a drag gesture to show browser controls. */
    public TripBuilder scrollPageUpWithGestureTo() {
        return runTo(
                () -> {
                    assertInPhase(Phase.ACTIVE);
                    View contentView = activityTabElement.value().getView();
                    float width = contentView.getWidth();
                    float height = contentView.getHeight();

                    int[] location = new int[2];
                    toolbarElement.value().getLocationOnScreen(location);
                    // Start the scroll with 5 additional height to avoid touching the toolbar.
                    float fromY = location[1] + toolbarElement.value().getBottom() + 5;
                    float toY = height;
                    TouchCommon.performDragNoFling(
                            mActivityElement.value(),
                            width / 2,
                            width / 2,
                            fromY,
                            toY,
                            /* stepCount= */ 50,
                            /* duration= */ 500);
                });
    }

    /** Trigger to scroll WebContents to the bottom. */
    public TripBuilder scrollToBottomTo() {
        return runJsTo("window.scrollTo(0, document.body.scrollHeight)")
                .waitForAnd(new ScrollToBottomCondition(webContentsElement));
    }

    /** Starts a Transition triggered by running |jsCode| in the WebContents. */
    public TripBuilder runJsTo(String jsCode) {
        return runTo(
                () -> {
                    try {
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                webContentsElement.value(), jsCode);
                    } catch (TimeoutException e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    /** Click the URL bar to enter the Omnibox. */
    public Pair<OmniboxFacility, SoftKeyboardFacility> openOmnibox(
            FakeOmniboxSuggestions fakeSuggestions) {
        OmniboxFacility omniboxFacility =
                new OmniboxFacility(/* incognito= */ mIsIncognito, fakeSuggestions);
        SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();

        urlBarElement.clickTo().enterFacilities(omniboxFacility, softKeyboard);
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
