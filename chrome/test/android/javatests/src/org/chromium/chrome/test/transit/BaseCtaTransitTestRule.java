// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Base class for integration tests that start {@link ChromeTabbedActivity}. */
@NullMarked
class BaseCtaTransitTestRule {
    protected final ChromeTabbedActivityTestRule mActivityTestRule;

    BaseCtaTransitTestRule() {
        mActivityTestRule = new ChromeTabbedActivityTestRule();
    }

    BaseCtaTransitTestRule(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    public ChromeTabbedActivityTestRule getActivityTestRule() {
        return mActivityTestRule;
    }

    public ChromeTabbedActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    public EmbeddedTestServer getTestServer() {
        return mActivityTestRule.getTestServer();
    }

    // TODO(crbug.com/406324209): Create WebPageStation#getWebContents() and replace these calls.
    public WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    // TODO(crbug.com/406324209): Create WebPageStation#runJavaScript() and replace these calls.
    public String runJavaScriptCodeInCurrentTab(String code) throws TimeoutException {
        return mActivityTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    public int tabsCount(boolean incognito) {
        return mActivityTestRule.tabsCount(incognito);
    }

    public Profile getProfile(boolean incognito) {
        return mActivityTestRule.getProfile(incognito);
    }

    // TODO(crbug.com/406324209): Develop WebPageStation#FrameInfoUpdatedCondition and replace these
    // calls.
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        mActivityTestRule.assertWaitForPageScaleFactorMatch(expectedScale);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(GURL url) {
        return mActivityTestRule.loadUrl(url);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(String url) {
        return mActivityTestRule.loadUrl(url);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(String url, long secondsToWait) {
        return mActivityTestRule.loadUrl(url, secondsToWait);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrlInTab(
            String url, int pageTransition, Tab tab, long secondsToWait) {
        return mActivityTestRule.loadUrlInTab(url, pageTransition, tab, secondsToWait);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrlInTab(String url, int pageTransition, Tab tab) {
        return mActivityTestRule.loadUrlInTab(url, pageTransition, tab);
    }

    // TODO(crbug.com/406324209): Use PageStation#openFakeLinkToWebPage() or #openFakeLink to
    // replace these calls.
    public Tab loadUrlInNewTab(String url) {
        return mActivityTestRule.loadUrlInNewTab(url);
    }

    // TODO(crbug.com/406324209): Use Public Transit in a case-by-case basis to replace these calls,
    // often with PageStation#openFakeLinkToWebPage().
    public Tab loadUrlInNewTab(final String url, final boolean incognito) {
        return mActivityTestRule.loadUrlInNewTab(url, incognito);
    }

    public Tab loadUrlInNewTab(
            final String url, final boolean incognito, final @TabLaunchType int launchType) {
        return mActivityTestRule.loadUrlInNewTab(url, incognito, launchType);
    }

    // TODO(crbug.com/406324209): Use PageStation#openNewIncognitoTabFast() to replace these calls.
    public Tab newIncognitoTabFromMenu() {
        return mActivityTestRule.newIncognitoTabFromMenu();
    }

    public KeyboardVisibilityDelegate getKeyboardDelegate() {
        return mActivityTestRule.getKeyboardDelegate();
    }

    public AppMenuCoordinator getAppMenuCoordinator() {
        return mActivityTestRule.getAppMenuCoordinator();
    }

    // TODO(crbug.com/406324209): Support tracking pause/resume state in Public Transit.
    public void resumeMainActivityFromLauncher() throws Exception {
        mActivityTestRule.resumeMainActivityFromLauncher();
    }

    // TODO(crbug.com/406324209): ChromeTabbedActivityTestRule#startActivityCompletely() already
    // calls this. Double check that callers are using those entry points and remove this.
    public void waitForActivityNativeInitializationComplete() {
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    // TODO(crbug.com/406324209): ChromeTabbedActivityTestRule#startActivityCompletely() already
    // calls this. Double check that callers are using those entry points and remove this.
    public void waitForActivityCompletelyLoaded() {
        mActivityTestRule.waitForActivityCompletelyLoaded();
    }

    // TODO(crbug.com/406324209): Support recreate() in Public Transit.
    public void recreateActivity() {
        mActivityTestRule.recreateActivity();
    }

    public EmbeddedTestServerRule getEmbeddedTestServerRule() {
        return mActivityTestRule.getEmbeddedTestServerRule();
    }

    // TODO(crbug.com/406324209): Use OmniboxFacility#typeText().
    public void typeInOmnibox(String text, boolean oneCharAtATime) throws InterruptedException {
        mActivityTestRule.typeInOmnibox(text, oneCharAtATime);
    }
}
