// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.support.test.InstrumentationRegistry;

import org.hamcrest.core.IsInstanceOf;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Utility functions for dealing with bookmarks in tests.
 */
public class BookmarkTestUtil {

    /**
     * Waits until the bookmark model is loaded, i.e. until
     * {@link BookmarkModel#isBookmarkModelLoaded()} is true.
     */
    public static void waitForBookmarkModelLoaded() {
        final BookmarkModel bookmarkModel = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile()); });

        CriteriaHelper.pollUiThread(bookmarkModel::isBookmarkModelLoaded);
    }

    /**  Do not read partner bookmarks in setUp(), so that the lazy reading is covered. */
    public static void readPartnerBookmarks(ChromeTabbedActivityTestRule activityTestRule) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PartnerBookmarksShim.kickOffReading(activityTestRule.getActivity()));
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    public static ChromeTabbedActivity waitForTabbedActivity() {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(ApplicationStatus.getLastTrackedFocusedActivity(),
                    IsInstanceOf.instanceOf(ChromeTabbedActivity.class));
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (ChromeTabbedActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static BookmarkEditActivity waitForEditActivity() {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(ApplicationStatus.getLastTrackedFocusedActivity(),
                    IsInstanceOf.instanceOf(BookmarkEditActivity.class));
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (BookmarkEditActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static BookmarkAddEditFolderActivity waitForAddEditFolderActivity() {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(ApplicationStatus.getLastTrackedFocusedActivity(),
                    IsInstanceOf.instanceOf(BookmarkAddEditFolderActivity.class));
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (BookmarkAddEditFolderActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static void waitForOfflinePageSaved(GURL url) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            List<OfflinePageItem> pages = OfflineTestUtil.getAllPages();
            String urlString = url.getSpec();
            for (OfflinePageItem item : pages) {
                if (urlString.startsWith(item.getUrl())) {
                    return true;
                }
            }
            return false;
        });
    }
}
