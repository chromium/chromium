// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedNewTabPage;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.TestNetworkClient;

/**
 * Custom ActivityTestRule for test using ChromeTabbedActivity
 */
public class ChromeTabbedActivityTestRule extends ChromeActivityTestRule<ChromeTabbedActivity> {
    // Response file for Feed's network stub.
    private static final String DEFAULT_FEED_TEST_RESPONSE_FILE_PATH =
            "/chrome/test/data/android/feed/feed_large.gcl.bin";

    private final String mFeedTestResponseFilePath;

    public ChromeTabbedActivityTestRule() {
        this(DEFAULT_FEED_TEST_RESPONSE_FILE_PATH);
    }

    /**
     * @param feedTestResponseFilePath The file path of the response that the feed library returns
     *                                 in tests.
     */
    public ChromeTabbedActivityTestRule(String feedTestResponseFilePath) {
        super(ChromeTabbedActivity.class);
        mFeedTestResponseFilePath = feedTestResponseFilePath;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        Statement tabbedActivityStatement = new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Setup Feed stubs if Feed is enabled.
                boolean feedStubsInitialized = false;
                if (!FeedNewTabPage.isDummy()) {
                    feedStubsInitialized = true;
                    TestNetworkClient client = new TestNetworkClient();
                    client.setNetworkResponseFile(
                            UrlUtils.getIsolatedTestFilePath(mFeedTestResponseFilePath));
                    FeedProcessScopeFactory.setTestNetworkClient(client);
                }

                base.evaluate();

                // Teardown the network stubs for Feed if they've been setup.
                if (feedStubsInitialized) FeedProcessScopeFactory.setTestNetworkClient(null);
            }
        };

        return super.apply(tabbedActivityStatement, description);
    }
}
