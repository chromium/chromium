// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.net.Uri;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.pdf.viewer.fragment.PdfViewerFragment;

import org.chromium.base.Log;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** The class responsible for setting up PdfPage. */
public class PdfCoordinator {
    private static final String TAG = "PdfCoordinator";
    private static boolean sSkipLoadPdfForTesting;
    private final View mView;
    private final FragmentManager mFragmentManager;
    private String mTabId;

    /** A unique id to identity the FragmentContainerView in the current PdfPage. */
    private final int mFragmentContainerViewId;

    /** The filepath of the pdf. It is null before download complete. */
    private String mPdfFilePath;

    /**
     * Whether the pdf has been loaded, despite of success or failure. This is used to ensure we
     * load the pdf at most once.
     */
    private boolean mIsPdfLoaded;

    private ChromePdfViewerFragment mChromePdfViewerFragment;

    private int mFindInPageCount;

    /**
     * Creates a PdfCoordinator for the PdfPage.
     *
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param filepath The pdf filepath.
     * @param tabId The id of the tab.
     */
    public PdfCoordinator(Profile profile, Activity activity, String filepath, int tabId) {
        mTabId = String.valueOf(tabId);
        mView = LayoutInflater.from(activity).inflate(R.layout.pdf_page, null);
        mView.setBackgroundColor(
                ChromeColors.getPrimaryBackgroundColor(activity, profile.isOffTheRecord()));
        mView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        loadPdfFile();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });
        View fragmentContainerView = mView.findViewById(R.id.pdf_fragment_container);
        mFragmentContainerViewId = View.generateViewId();
        fragmentContainerView.setId(mFragmentContainerViewId);
        mFragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        // TODO(b/360717802): Reuse fragment from savedInstance.
        Fragment fragment = mFragmentManager.findFragmentByTag(mTabId);
        if (fragment != null) {
            mFragmentManager.beginTransaction().remove(fragment).commitAllowingStateLoss();
        }
        // Create PdfViewerFragment to start showing the loading spinner.
        mChromePdfViewerFragment = new ChromePdfViewerFragment();
        loadPdfFile(filepath);
    }

    /** The class responsible for rendering pdf document. */
    public static class ChromePdfViewerFragment extends PdfViewerFragment {
        /** Whether the pdf has been loaded successfully. */
        boolean mIsLoadDocumentSuccess;

        /** The timestamp when the pdf document starts to load. */
        long mDocumentLoadStartTimestamp;

        @Override
        public void onLoadDocumentSuccess() {
            mIsLoadDocumentSuccess = true;
            PdfUtils.recordPdfLoadTime(SystemClock.elapsedRealtime() - mDocumentLoadStartTimestamp);
            PdfUtils.recordPdfLoadResult(true);
        }

        @Override
        public void onLoadDocumentError(@NonNull Throwable throwable) {
            PdfUtils.recordPdfLoadResult(false);
        }
    }

    /** Returns the intended view for PdfPage tab. */
    View getView() {
        return mView;
    }

    /**
     * Show pdf specific find in page UI.
     *
     * @return whether the pdf specific find in page UI is shown.
     */
    boolean findInPage() {
        if (mChromePdfViewerFragment != null && mChromePdfViewerFragment.mIsLoadDocumentSuccess) {
            mChromePdfViewerFragment.setTextSearchActive(true);
            PdfUtils.recordFindInPage(mFindInPageCount++);
            return true;
        }
        return false;
    }

    /**
     * Called after a pdf page has been removed from the view hierarchy and will no longer be used.
     */
    void destroy() {
        if (mChromePdfViewerFragment == null) {
            PdfUtils.recordHasFilepathWithoutFragmentOnDestroy(mPdfFilePath != null);
            Log.w(TAG, "Fragment is null when pdf page is destroyed.");
            return;
        }
        if (!mFragmentManager.isDestroyed()) {
            mFragmentManager
                    .beginTransaction()
                    .remove(mChromePdfViewerFragment)
                    .commitAllowingStateLoss();
        }
        mChromePdfViewerFragment = null;
    }

    /**
     * Called after pdf download complete.
     *
     * @param pdfFilePath The filepath of the downloaded pdf document.
     */
    void onDownloadComplete(String pdfFilePath) {
        loadPdfFile(pdfFilePath);
    }

    /** Returns the filepath of the pdf document. */
    String getFilepath() {
        return mPdfFilePath;
    }

    private void loadPdfFile(String pdfFilePath) {
        mPdfFilePath = pdfFilePath;
        loadPdfFile();
    }

    private void loadPdfFile() {
        if (mIsPdfLoaded) {
            return;
        }
        if (mPdfFilePath == null) {
            return;
        }
        if (mView.getParent() == null) {
            return;
        }
        Uri uri = PdfUtils.getUriFromFilePath(mPdfFilePath);
        if (uri != null) {
            try {
                if (!sSkipLoadPdfForTesting) {
                    // Committing the fragment
                    // TODO(b/360717802): Reuse fragment from savedInstance.
                    FragmentTransaction transaction = mFragmentManager.beginTransaction();
                    transaction.add(mFragmentContainerViewId, mChromePdfViewerFragment, mTabId);
                    transaction.commitAllowingStateLoss();
                    mFragmentManager.executePendingTransactions();
                    PdfUtils.recordPdfLoad();
                    mChromePdfViewerFragment.mDocumentLoadStartTimestamp =
                            SystemClock.elapsedRealtime();
                    mChromePdfViewerFragment.setDocumentUri(uri);
                }
            } catch (Exception e) {
                Log.e(TAG, "Load pdf fails.", e);
            } finally {
                mIsPdfLoaded = true;
            }
        } else {
            // TODO(b/348712628): show some error UI when content URI is null.
            Log.e(TAG, "Uri is null.");
        }
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }

    static void skipLoadPdfForTesting(boolean skipLoadPdfForTesting) {
        sSkipLoadPdfForTesting = skipLoadPdfForTesting;
    }
}
