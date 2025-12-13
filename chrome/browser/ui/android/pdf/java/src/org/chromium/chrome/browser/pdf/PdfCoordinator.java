// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ProgressBar;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.pdf.PdfSandboxHandle;
import androidx.pdf.SandboxedPdfLoader;
import androidx.pdf.viewer.fragment.PdfViewerFragment;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfLoadResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.MimeTypeUtils;

/**
 * The class responsible for setting up PdfPage.
 *
 * <p>Lint suppression for NewApi is added because we are using PdfViewerFragment and inline pdf
 * support is enabled via PdfUtils#shouldOpenPdfInline.
 */
@SuppressLint("NewApi")
@NullMarked
public class PdfCoordinator {
    private static final String TAG = "PdfCoordinator";

    /**
     * The timestamp when the last pdf document starts to load. Used to calculate the elapsed time
     * between two pdf loads.
     */
    private static long sLastPdfLoadTimestamp;

    private static boolean sSkipLoadPdfForTesting;
    private final View mView;
    private final FragmentManager mFragmentManager;
    private final Activity mActivity;
    private final String mTabId;

    /** A unique id to identity the FragmentContainerView in the current PdfPage. */
    private final int mFragmentContainerViewId;

    /** The filepath of the pdf. It is null before download complete. */
    private @Nullable String mPdfFilePath;

    /**
     * Whether the pdf has been loaded, despite of success or failure. This is used to ensure we
     * load the pdf at most once.
     */
    private boolean mIsPdfLoaded;

    /** Uri of the pdf document. Generated when the pdf is ready to load. */
    private @Nullable Uri mUri;

    @VisibleForTesting public ChromePdfViewerFragment mChromePdfViewerFragment;

    private int mFindInPageCount;

    /** ProgressBar to be shown during PDF download. */
    private final ProgressBar mProgressBar;

    /** A PdfSandboxHandle representing the active pdf session. */
    private PdfSandboxHandle mPdfSandboxHandle;

    /**
     * Creates a PdfCoordinator for the PdfPage.
     *
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param filepath The pdf filepath.
     * @param tabId The id of the tab.
     */
    public PdfCoordinator(
            Profile profile, Activity activity, @Nullable String filepath, int tabId) {
        mActivity = activity;
        mTabId = String.valueOf(tabId);
        mView = LayoutInflater.from(activity).inflate(R.layout.pdf_page, null);
        mProgressBar = mView.findViewById(R.id.progress_bar);
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
        // Start pdf library initialization. This prepares pdf resources ahead of time, so that pdf
        // could be loaded faster when documentUri is set.
        mPdfSandboxHandle = SandboxedPdfLoader.startInitialization(activity);
        // PDF is downloading when the filepath is null.
        if (filepath == null) {
            mProgressBar.setVisibility(View.VISIBLE);
        }
        loadPdfFile(filepath);
    }

    /** The class responsible for rendering pdf document. */
    public static class ChromePdfViewerFragment extends PdfViewerFragment {
        /** Whether the pdf has been loaded successfully. */
        @VisibleForTesting public boolean mIsLoadDocumentSuccess;

        /** Whether the pdf has emitted any load error. */
        boolean mIsLoadDocumentError;

        /** The timestamp when the pdf document starts to load. */
        long mDocumentLoadStartTimestamp;

        @Override
        public void onLoadDocumentSuccess() {
            if (mDocumentLoadStartTimestamp <= 0) {
                return;
            }
            // There should be only one success callback for each pdf. Add this confidence check to
            // be consistent with the error callback.
            if (!mIsLoadDocumentSuccess) {
                PdfUtils.recordPdfLoadTimeFirstPaired(
                        SystemClock.elapsedRealtime() - mDocumentLoadStartTimestamp);
                PdfUtils.recordPdfLoadResultDetail(PdfLoadResult.SUCCESS);
            }
            mIsLoadDocumentSuccess = true;
        }

        @Override
        public void onLoadDocumentError(Throwable throwable) {
            if (mDocumentLoadStartTimestamp <= 0) {
                return;
            }
            // Only record the first error emitted.
            if (!mIsLoadDocumentError) {
                PdfUtils.recordPdfLoadResultDetail(PdfLoadResult.ERROR);
            }
            mIsLoadDocumentError = true;
        }
    }

    /** Returns the intended view for PdfPage tab. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public View getView() {
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
    @SuppressWarnings({"NullAway"})
    void destroy() {
        mPdfSandboxHandle.close();
        mPdfSandboxHandle = null;
        if (mChromePdfViewerFragment == null) {
            Log.w(TAG, "Fragment is null when pdf page is destroyed.");
            return;
        }
        // Record abort when there is paired pdf load but no load success or error.
        if (mChromePdfViewerFragment.mDocumentLoadStartTimestamp > 0
                && !mChromePdfViewerFragment.mIsLoadDocumentSuccess
                && !mChromePdfViewerFragment.mIsLoadDocumentError) {
            PdfUtils.recordPdfLoadResultDetail(PdfLoadResult.ABORT);
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
    @Nullable String getFilepath() {
        return mPdfFilePath;
    }

    private void loadPdfFile(@Nullable String pdfFilePath) {
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
        mUri = PdfUtils.getUriFromFilePath(mPdfFilePath);
        if (mUri != null) {
            // TODO(crbug.com/418075119): Minimize the try catch block.
            try {
                if (!sSkipLoadPdfForTesting) {
                    // Committing the fragment
                    // TODO(crbug.com/360717802): Reuse fragment from savedInstance.
                    FragmentTransaction transaction = mFragmentManager.beginTransaction();
                    transaction.add(mFragmentContainerViewId, mChromePdfViewerFragment, mTabId);
                    transaction.commitAllowingStateLoss();
                    mFragmentManager.executePendingTransactions();
                    PdfUtils.recordPdfLoad();
                    long currentTime = SystemClock.elapsedRealtime();
                    mChromePdfViewerFragment.mDocumentLoadStartTimestamp = currentTime;
                    if (sLastPdfLoadTimestamp > 0) {
                        PdfUtils.recordPdfLoadInterval(currentTime - sLastPdfLoadTimestamp);
                    }
                    sLastPdfLoadTimestamp = currentTime;
                    mProgressBar.setVisibility(View.GONE);
                    mChromePdfViewerFragment.setDocumentUri(mUri);
                }
            } catch (Exception e) {
                Log.e(TAG, "Load pdf fails.", e);
            } finally {
                mIsPdfLoaded = true;
            }
        } else {
            // TODO(crbug.com/348712628): show some error UI when content URI is null.
            Log.e(TAG, "Uri is null.");
        }
    }

    @Nullable String requestAssistContent(String filename, boolean isWorkProfile) {
        if (mUri == null) {
            return null;
        }
        String structuredData;
        try {
            structuredData =
                    new JSONObject()
                            .put(
                                    "file_metadata",
                                    new JSONObject()
                                            .put("file_uri", mUri.toString())
                                            .put("mime_type", MimeTypeUtils.PDF_MIME_TYPE)
                                            .put("file_name", filename)
                                            .put("is_work_profile", isWorkProfile))
                            .toString();
        } catch (JSONException e) {
            return null;
        }
        var assistantPackageName = PackageUtils.getDefaultAssistantPackageName(mActivity);
        PdfUtils.recordGetAssistantPackageResult(assistantPackageName != null);
        if (assistantPackageName != null) {
            mActivity.grantUriPermission(
                    assistantPackageName, mUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        PdfUtils.recordIsWorkProfile(isWorkProfile);
        return structuredData;
    }

    @Nullable Uri getUri() {
        return mUri;
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }

    static void skipLoadPdfForTesting(boolean skipLoadPdfForTesting) {
        sSkipLoadPdfForTesting = skipLoadPdfForTesting;
    }
}
