// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Utility functions for dealing with bookmarks in tests.
 */
public class BookmarkTestUtil {

    /**
     * Waits until the bookmark model is loaded, i.e. until
     * {@link BookmarkModel#isBookmarkModelLoaded()} is true.
     */
    public static void waitForBookmarkModelLoaded() {
        final BookmarkModel bookmarkModel =
                TestThreadUtils.runOnUiThreadBlockingNoException(BookmarkModel::new);

        CriteriaHelper.pollUiThread(bookmarkModel::isBookmarkModelLoaded);

        TestThreadUtils.runOnUiThreadBlocking(bookmarkModel::destroy);
    }

    /**
     * Show BookmarkManager and wait till the bookmark model is loaded.
     */
    public static void showBookmarkManager(ChromeActivity activity) {
        TestThreadUtils.runOnUiThreadBlocking(() -> BookmarkUtils.showBookmarkManager(activity));
        waitForBookmarkModelLoaded();
    }
}
