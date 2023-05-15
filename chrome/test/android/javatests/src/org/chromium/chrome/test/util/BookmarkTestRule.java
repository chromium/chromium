// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import androidx.annotation.Nullable;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * This test rule destroys BookmarkActivity opened with showBookmarkManager on phone.
 * On tablet, the bookmark manager is a native page so the manager will be destroyed
 * with the parent chromeActivity.
 */
public class BookmarkTestRule implements TestRule {
    @Nullable
    private BookmarkActivity mBookmarkActivity;

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
     * Shows the bookmark manager on screen.
     */
    public void showBookmarkManager(ChromeActivity chromeActivity) {
        // BookmarkActivity is only opened on phone, it is a native page on tablet.
        if (chromeActivity.isTablet()) {
            showBookmarkManagerInternal(chromeActivity);
        } else {
            mBookmarkActivity = ActivityTestUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), BookmarkActivity.class,
                    () -> showBookmarkManagerInternal(chromeActivity));
        }
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    /**
     * Returns the bookmark activity.
     */
    @Nullable
    public BookmarkActivity getBookmarkActivity() {
        return mBookmarkActivity;
    }

    private void showBookmarkManagerInternal(ChromeActivity chromeActivity) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> BookmarkUtils.showBookmarkManager(chromeActivity, /*isIncognito=*/false));
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
