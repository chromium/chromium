// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * This test rule destroys BookmarkActivity opened with showBookmarkManager on phone.
 * On tablet, the bookmark manager is a native page so the manager will be destroyed
 * with the parent chromeActivity.
 */
public class BookmarkTestRule implements TestRule {
    @Nullable private BookmarkActivity mBookmarkActivity;

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    statement.evaluate();
                } finally {
                    tearDown();
                }
            }
        };
    }

    /**
     * Shows the bookmark manager on screen. On tablets, it should only be called if there's an
     * active tab to load the bookmarks manager in.
     */
    public BookmarkManagerCoordinator showBookmarkManager(ChromeActivity chromeActivity) {
        BookmarkManagerCoordinator coordinator;
        // BookmarkActivity is only opened on phone, it is a native page on tablet.
        if (chromeActivity.isTablet()) {
            // Wait for the bookmark native page to load to make sure that the reference to
            // BookmarkManagerCoordinator is valid.
            CallbackHelper callbackHelper = new CallbackHelper();
            EmptyTabObserver obs =
                    new EmptyTabObserver() {
                        @Override
                        public void onPageLoadFinished(Tab tab, GURL url) {
                            NativePage nativePage = tab.getNativePage();
                            if (nativePage != null
                                    && nativePage.getHost().equals(UrlConstants.BOOKMARKS_HOST)) {
                                callbackHelper.notifyCalled();
                            }
                        }
                    };
            Tab tab = chromeActivity.getActivityTab();
            assert tab != null;

            runOnUiThreadBlocking(() -> tab.addObserver(obs));
            showBookmarkManagerInternal(chromeActivity);
            try {
                callbackHelper.waitForOnly();
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
            runOnUiThreadBlocking(() -> tab.removeObserver(obs));

            coordinator = ((BookmarkPage) tab.getNativePage()).getManagerForTesting();

        } else {
            mBookmarkActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            BookmarkActivity.class,
                            () -> showBookmarkManagerInternal(chromeActivity));
            coordinator = mBookmarkActivity.getManagerForTesting();
        }
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        return coordinator;
    }

    /** Returns the bookmark activity. */
    public @Nullable BookmarkActivity getBookmarkActivity() {
        return mBookmarkActivity;
    }

    private void showBookmarkManagerInternal(ChromeActivity chromeActivity) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> BookmarkUtils.showBookmarkManager(chromeActivity, /* isIncognito= */ false));
    }

    /**
     * Tears down the BookmarkActivity opened on phone. On tablet, it is
     * a native page so the bookmarks will finish with the chromeActivity.
     */
    private void tearDown() throws Exception {
        if (mBookmarkActivity != null) {
            ApplicationTestUtils.finishActivity(mBookmarkActivity);
        }
    }
}
