// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * This test rule destroys BookmarkActivity opened with showBookmarkManager on phone. On tablet, the
 * bookmark manager is a native page so the manager will be destroyed with the parent
 * ChromeActivity.
 */
public class BookmarkTestRule implements TestRule {
    private ChromeActivity mHostActivity;
    private BookmarkManagerOpener mBookmarkManagerOpener;
    @Nullable private BookmarkActivity mBookmarkActivity;
    private BookmarkManagerCoordinator mCoordinator;
    private BookmarkDelegate mDelegate;
    private RecyclerView mItemsContainer;

    private boolean mModelLoaded;
    private BookmarkId mRootFolder;
    private BookmarkId mDesktopFolder;
    private BookmarkId mMobileFolder;
    private BookmarkId mReadingListFolder;

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
     * active tab to load the bookmarks manager in. Does not load the bookmark model, either use
     * #showBookmarkManagerAndLoadModel to group the operations, or use #loadModel separately.
     */
    public BookmarkManagerCoordinator showBookmarkManager(ChromeActivity chromeActivity) {
        mHostActivity = chromeActivity;
        // BookmarkActivity is only opened on phone, it is a native page on tablet.
        if (chromeActivity.isTablet()) {
            Tab tab = chromeActivity.getActivityTab();
            assert tab != null;
            // Wait for the bookmark native page to load to make sure that the reference to
            // BookmarkManagerCoordinator is valid.
            CallbackHelper callbackHelper = setupBookmarkTabletWaitCondition();
            showBookmarkManagerInternal(chromeActivity);
            try {
                callbackHelper.waitForNext();
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
            mCoordinator = ((BookmarkPage) tab.getNativePage()).getManagerForTesting();
        } else {
            mBookmarkActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            BookmarkActivity.class,
                            () -> showBookmarkManagerInternal(chromeActivity));
            mCoordinator = mBookmarkActivity.getManagerForTesting();
        }
        mDelegate = mCoordinator.getBookmarkDelegateForTesting();
        mItemsContainer = mCoordinator.getRecyclerViewForTesting();
        // Load the bookmark model.
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        runOnUiThreadBlocking(
                () -> {
                    BookmarkModel model = chromeActivity.getBookmarkModelForTesting();
                    mRootFolder = model.getRootFolderId();
                    mDesktopFolder = model.getDesktopFolderId();
                    mMobileFolder = model.getMobileFolderId();
                    mReadingListFolder = model.getLocalOrSyncableReadingListFolder();
                });
        mModelLoaded = true;
        return mCoordinator;
    }

    /** Opens the given folder and waits for it to load. */
    public void openFolder(BookmarkId folder) {
        if (mHostActivity.isTablet()) {
            CallbackHelper callbackHelper = setupBookmarkTabletWaitCondition();
            runOnUiThreadBlocking(() -> mDelegate.openFolder(folder));
            try {
                callbackHelper.waitForNext();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            runOnUiThreadBlocking(() -> mDelegate.openFolder(folder));
        }
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
    }

    /** Returns the bookmark activity. */
    public @Nullable BookmarkActivity getBookmarkActivity() {
        return mBookmarkActivity;
    }

    /** Returns the root folder. */
    public BookmarkId getRootFolder() {
        assert mModelLoaded;
        return mRootFolder;
    }

    /** Returns the desktop (aka bookmarks bar) folder. */
    public BookmarkId getDesktopFolder() {
        assert mModelLoaded;
        return mDesktopFolder;
    }

    /** Returns the mobile folder. */
    public BookmarkId getMobileFolder() {
        assert mModelLoaded;
        return mMobileFolder;
    }

    /** Returns the reading list folder. */
    public BookmarkId getReadingListFolder() {
        assert mModelLoaded;
        return mReadingListFolder;
    }

    private void showBookmarkManagerInternal(ChromeActivity chromeActivity) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkManagerOpener = new BookmarkManagerOpenerImpl();
                    mBookmarkManagerOpener.showBookmarkManager(
                            chromeActivity,
                            chromeActivity.getProfileProviderSupplier().get().getOriginalProfile());
                });
    }

    /**
     * Tears down the BookmarkActivity opened on phone. On tablet, it is a native page so the
     * bookmarks will finish with the chromeActivity.
     */
    private void tearDown() throws Exception {
        if (mBookmarkActivity != null) {
            ApplicationTestUtils.finishActivity(mBookmarkActivity);
        }
    }

    private CallbackHelper setupBookmarkTabletWaitCondition() {
        CallbackHelper callbackHelper = new CallbackHelper();
        Tab tab = mHostActivity.getActivityTab();
        assert tab != null;
        EmptyTabObserver obs =
                new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        NativePage nativePage = tab.getNativePage();
                        if (nativePage != null
                                && nativePage.getHost().equals(UrlConstants.BOOKMARKS_HOST)) {
                            callbackHelper.notifyCalled();
                            tab.removeObserver(this);
                        }
                    }
                };
        runOnUiThreadBlocking(() -> tab.addObserver(obs));
        return callbackHelper;
    }
}
