// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.core.IsInstanceOf;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.ExecutionException;

/** Utility functions for dealing with bookmarks in tests. */
public class BookmarkTestUtil {
    /**
     * Loads an empty partner bookmarks folder for testing. The partner bookmarks folder will appear
     * in the mobile bookmarks folder.
     */
    public static void loadEmptyPartnerBookmarksForTesting(BookmarkModel bookmarkModel) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });
        waitForBookmarkModelLoaded();
    }

    /** Opens the root folder in the bookmarks manager. */
    public static void openRootFolder(
            RecyclerView recyclerView,
            BookmarkDelegate bookmarkDelegate,
            BookmarkModel bookmarkModel) {
        waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> bookmarkDelegate.openFolder(bookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
    }

    /** Opens the mobile bookmarks folder in the bookmarks manager. */
    // TODO(crbug.com/40276708): Remove use of waitForIdleSync here.
    public static void openMobileBookmarks(
            RecyclerView recyclerView,
            BookmarkDelegate bookmarkDelegate,
            BookmarkModel bookmarkModel) {
        openRootFolder(recyclerView, bookmarkDelegate, bookmarkModel);

        onView(withText("Mobile bookmarks")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /** Opens the reading list folder in the bookmarks manager. */
    // TODO(crbug.com/40276708): Remove use of waitForIdleSync here.
    public static void openReadingList(
            RecyclerView recyclerView,
            BookmarkDelegate bookmarkDelegate,
            BookmarkModel bookmarkModel) {
        openRootFolder(recyclerView, bookmarkDelegate, bookmarkModel);

        onView(withText("Reading list")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /** Adds a folder with the given title and parent. */
    public static BookmarkId addFolder(
            ChromeTabbedActivityTestRule activityTestRule,
            BookmarkModel bookmarkModel,
            String title,
            BookmarkId parent)
            throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(activityTestRule);
        return ThreadUtils.runOnUiThreadBlocking(() -> bookmarkModel.addFolder(parent, 0, title));
    }

    /**
     * Waits until the bookmark model is loaded, i.e. until {@link
     * BookmarkModel#isBookmarkModelLoaded()} is true.
     */
    public static void waitForBookmarkModelLoaded() {
        final BookmarkModel bookmarkModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return BookmarkModel.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                        });

        CriteriaHelper.pollUiThread(bookmarkModel::isBookmarkModelLoaded);
    }

    /** Do not read partner bookmarks in setUp(), so that the lazy reading is covered. */
    public static void readPartnerBookmarks(ChromeTabbedActivityTestRule activityTestRule) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> PartnerBookmarksShim.kickOffReading(activityTestRule.getActivity()));
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    public static ChromeTabbedActivity waitForTabbedActivity() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getLastTrackedFocusedActivity(),
                            IsInstanceOf.instanceOf(ChromeTabbedActivity.class));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (ChromeTabbedActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static BookmarkActivity waitForBookmarkActivity() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getLastTrackedFocusedActivity(),
                            IsInstanceOf.instanceOf(BookmarkActivity.class));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (BookmarkActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static BookmarkEditActivity waitForEditActivity() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getLastTrackedFocusedActivity(),
                            IsInstanceOf.instanceOf(BookmarkEditActivity.class));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (BookmarkEditActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static BookmarkFolderPickerActivity waitForFolderPickerActivity() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getLastTrackedFocusedActivity(),
                            IsInstanceOf.instanceOf(BookmarkFolderPickerActivity.class));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return (BookmarkFolderPickerActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    public static void waitForOfflinePageSaved(GURL url) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
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

    /**
     * Returns the {@link ViewInteraction} for a bookmark row with the given text and account-ness.
     * The return value can be used to make assertions about.
     */
    public static ViewInteraction getRecyclerRowViewInteraction(
            String text, boolean isAccountBookmark) {
        ViewMatchers.Visibility visibility =
                isAccountBookmark ? ViewMatchers.Visibility.GONE : ViewMatchers.Visibility.VISIBLE;
        return onView(
                allOf(
                        withId(R.id.container),
                        hasDescendant(withText(text)),
                        hasDescendant(
                                allOf(
                                        withId(R.id.local_bookmark_image),
                                        withEffectiveVisibility(visibility)))));
    }
}
