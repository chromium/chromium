// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

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
}
