// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;

import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.pdf.PdfPoint;
import androidx.pdf.PdfSandboxHandle;
import androidx.pdf.SandboxedPdfLoader;
import androidx.pdf.content.ExternalLink;
import androidx.pdf.view.PdfView;
import androidx.pdf.viewer.fragment.PdfViewerFragment;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.BundleUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfLoadResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * The class responsible for setting up PdfPage.
 *
 * <p>Lint suppression for NewApi is added because we are using PdfViewerFragment and inline pdf
 * support is enabled via PdfUtils#shouldOpenPdfInline.
 */
@SuppressLint("NewApi")
@NullMarked
public class PdfCoordinator
        implements PdfCoordinatorInterface, PdfActionsDelegate, PdfToolbarActionsDelegate {
    private static final String TAG = "PdfCoordinator";
    private static final int PAGE_TRANSITION_TYPE = PageTransition.LINK;

    // PDF link annotations are untrusted input (ISO 32000-1 §12.6.4.7 leaves scheme policy
    // to the viewer). Restrict to schemes that have a meaningful web-navigation or
    // communication semantic, mirroring the defaults used by pdf.js
    // (PDFLinkService.getAnchorUrl) and Adobe Reader's Trust Manager. Blocks dangerous
    // schemes such as javascript:, data:, file:, content:, intent:, chrome:, devtools:.
    private static final Set<String> ALLOWED_LINK_SCHEMES =
            Set.of("http", "https", "mailto", "tel", "ftp");

    static final String JSON_KEY_FILE_METADATA = "file_metadata";
    static final String JSON_KEY_FILE_URI = "file_uri";
    static final String JSON_KEY_MIME_TYPE = "mime_type";
    static final String JSON_KEY_FILE_NAME = "file_name";
    static final String JSON_KEY_IS_WORK_PROFILE = "is_work_profile";

    /**
     * The timestamp when the last pdf document starts to load. Used to calculate the elapsed time
     * between two pdf loads.
     */
    private static long sLastPdfLoadTimestamp;

    private static boolean sSkipLoadPdfForTesting;

    private final Activity mActivity;
    private final FragmentManager mFragmentManager;
    private final View mView;

    /** ProgressBar to be shown during PDF download. */
    private final ProgressBar mProgressBar;

    private final NativePageHost mNativePageHost;

    private final String mTabId;
    private String mTitle;
    private final String mUrl;
    private final boolean mIsIncognito;

    /** A unique id to identity the FragmentContainerView in the current PdfPage. */
    private final int mFragmentContainerViewId;

    @SuppressWarnings("UnusedVariable")
    private @Nullable PdfSelectionCoordinator mPdfSelectionCoordinator;

    private final @Nullable PdfToolbarCoordinator mToolbarCoordinator;

    /**
     * Temporarily holds PdfViewerFragment's Views object that were added to current Tab's PdfPage
     * Views.
     */
    private final List<View> mPdfFragmentViews;

    /** The filepath of the pdf. It is null before download complete. */
    private @Nullable String mPdfFilePath;

    /** Uri of the pdf document. Generated when the pdf is ready to load. */
    private @Nullable Uri mUri;

    /** A PdfSandboxHandle representing the active pdf session. */
    private @Nullable PdfSandboxHandle mPdfSandboxHandle;

    /**
     * Whether the pdf has been loaded, despite of success or failure. This is used to ensure we
     * load the pdf at most once.
     */
    private boolean mIsPdfLoaded;

    private int mFindInPageCount;

    @VisibleForTesting public ChromePdfViewerFragment mChromePdfViewerFragment;

    /**
     * Creates a PdfCoordinator for the PdfPage.
     *
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param filepath The pdf filepath.
     * @param title The pdf title.
     * @param tabId The id of the tab.
     * @param url The url of the pdf.
     */
    public PdfCoordinator(
            NativePageHost host,
            Profile profile,
            Activity activity,
            @Nullable String filepath,
            String title,
            int tabId,
            String url,
            List<View> pdfFragmentViews) {
        mActivity = activity;
        mTabId = String.valueOf(tabId);
        mNativePageHost = host;
        mIsIncognito = profile.isOffTheRecord();
        mTitle = title;
        mUrl = url;
        Context contextForInflation =
                BundleUtils.createContextForInflation(activity, OnDemandModule.SPLIT_NAME);
        mView = LayoutInflater.from(contextForInflation).inflate(R.layout.pdf_page, null);
        mPdfFragmentViews = pdfFragmentViews;
        mProgressBar = mView.findViewById(R.id.progress_bar);
        mView.setBackgroundColor(
                ChromeColors.getPrimaryBackgroundColor(activity, profile.isOffTheRecord()));
        mView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        relocatePdfPageViews();
                        loadPdfFile();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });
        mFragmentContainerViewId = R.id.pdf_fragment_container;
        mFragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        Fragment fragment = mFragmentManager.findFragmentByTag(mTabId);
        if (fragment != null) {
            mChromePdfViewerFragment = (ChromePdfViewerFragment) fragment;
        } else {
            mChromePdfViewerFragment = new ChromePdfViewerFragment(this);
            mChromePdfViewerFragment.setViewTag(mTabId);
            // Start pdf library initialization. This prepares pdf resources ahead of time, so that
            // pdf could be loaded faster when documentUri is set.
            mPdfSandboxHandle = SandboxedPdfLoader.startInitialization(activity);
            // PDF is downloading when the filepath is null.
            if (filepath == null) {
                mProgressBar.setVisibility(View.VISIBLE);
            }
            loadPdfFile(filepath);
        }
        mToolbarCoordinator =
                PdfUtils.isInlinePdfV2Enabled() ? new PdfToolbarCoordinator(mView, this) : null;
        if (!PdfUtils.isInlinePdfV2Enabled()) {
            View container = mView.findViewById(R.id.pdf_fragment_container);
            ConstraintLayout.LayoutParams params =
                    (ConstraintLayout.LayoutParams) container.getLayoutParams();
            params.topToBottom = ConstraintLayout.LayoutParams.UNSET;
            params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
            container.setLayoutParams(params);
        }
    }

    /**
     * Move the PdfViewerFragment's views wrongly added to the current PdfPage view to the right one
     * using the Tab ID as unique identifier.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void relocatePdfPageViews() {
        ViewGroup fcv = mView.findViewById(R.id.pdf_fragment_container);
        if (fcv.getChildCount() == 1) return;

        for (int i = fcv.getChildCount() - 1; i >= 0; i--) {
            View child = fcv.getChildAt(i);
            if (!mTabId.equals(child.getTag())) {
                fcv.removeView(child);
                mPdfFragmentViews.add(child);
            }
        }
        for (int i = mPdfFragmentViews.size() - 1; i >= 0; i--) {
            View child = mPdfFragmentViews.get(i);
            if (mTabId.equals(child.getTag())) {
                fcv.addView(child);
                mPdfFragmentViews.remove(child);
            }
        }
    }

    /** The class responsible for rendering pdf document. */
    public static class ChromePdfViewerFragment extends PdfViewerFragment {

        private static final String KEY_VIEW_TAG = "view_tag";
        private @Nullable PdfActionsDelegate mDelegate;
        private @Nullable PdfView mPdfView;

        @Nullable private String mViewTag;

        public void setPdfViewForTesting(PdfView pdfView) {
            this.mPdfView = pdfView;
        }

        @Override
        public void onPdfViewCreated(PdfView pdfView) {
            super.onPdfViewCreated(pdfView);
            mPdfView = pdfView;

            if (getView() != null && mViewTag != null) getView().setTag(mViewTag);
            // TODO(crbug.com/498644542): call getPageCount() within onLoadDocumentSuccess()
            if (!PdfUtils.isInlinePdfV2Enabled() || mDelegate == null) {
                return;
            }
            mDelegate.loadPdfSelectionCoordinator(pdfView);
            final PdfView capturedView = pdfView;
            final PdfActionsDelegate delegate = mDelegate;

            // Add a one-time listener to track total page count and remove itself afterwards.
            // This listener is necessary because getPdfDocument() can return null up until the
            // viewport is changed.
            capturedView.addOnViewportChangedListener(
                    new PdfView.OnViewportChangedListener() {
                        @Override
                        public void onViewportChanged(
                                int firstVisiblePage,
                                int visiblePagesCount,
                                SparseArray pageLocations,
                                float zoomLevel) {
                            if (capturedView.getPdfDocument() != null) {
                                // Post to the UI thread to avoid removing the listener while
                                // androidx.pdf.view.PdfView is notifying its listeners, which can
                                // throw an IndexOutOfBoundsException error.
                                ThreadUtils.postOnUiThread(
                                        () -> capturedView.removeOnViewportChangedListener(this));
                                delegate.onDocumentLoaded(
                                        capturedView.getPdfDocument().getPageCount());
                            }
                        }
                    });
            // Add a persistent listener to track page changes.
            capturedView.addOnViewportChangedListener(
                    (firstVisiblePage, visiblePagesCount, pageLocations, zoomLevel) ->
                            delegate.onViewportChanged(firstVisiblePage, zoomLevel));
        }

        /** Public no-arg constructor for FragmentManager. */
        public ChromePdfViewerFragment() {}

        public ChromePdfViewerFragment(PdfActionsDelegate handler) {
            mDelegate = handler;
        }

        /** Whether the pdf has been loaded successfully. */
        @VisibleForTesting public boolean mIsLoadDocumentSuccess;

        /** Whether the pdf has emitted any load error. */
        boolean mIsLoadDocumentError;

        /** The timestamp when the pdf document starts to load. */
        long mDocumentLoadStartTimestamp;

        public void setViewTag(String tag) {
            mViewTag = tag;
        }

        @Override
        public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
            super.onViewCreated(view, savedInstanceState);
            if (savedInstanceState != null) {
                mViewTag = savedInstanceState.getString(KEY_VIEW_TAG, null);
                if (getView() != null) getView().setTag(mViewTag);
            }
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
            outState.putString(KEY_VIEW_TAG, mViewTag);
        }

        @Override
        public boolean onLinkClicked(ExternalLink externalLink) {
            if (mDelegate == null) {
                return false;
            }
            return mDelegate.onLinkClicked(externalLink.getUri());
        }

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

        void scrollToPage(int pageIndex) {
            if (mPdfView != null) {
                // 1. Get the current height of the view in pixels.
                float viewHeightPx = mPdfView.getHeight();

                // 2. Get the current zoom level.
                float currentZoom = mPdfView.getZoom();

                // 3. Calculate half of the viewport height in PDF content units (points).
                // Formula: (Pixels / 2) / Zoom = Content Points
                // When the viewer "centers" this point, the top of the page (0,0)
                // will be pushed exactly to the top of the screen.
                float yOffsetPoints = (viewHeightPx / 2f) / currentZoom;

                // 4. Use the single-argument scrollToPosition.
                // The internal logic will center this offset, resulting in a top-aligned page.
                mPdfView.scrollToPosition(new PdfPoint(pageIndex, 0f, yOffsetPoints));
            }
        }

        void zoomTo(float zoomLevel) {
            if (mPdfView != null) {
                mPdfView.setZoom(zoomLevel);
            }
        }
    }

    /** Returns the intended view for PdfPage tab. */
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public View getView() {
        return mView;
    }

    /**
     * Show pdf specific find in page UI.
     *
     * @return whether the pdf specific find in page UI is shown.
     */
    @Override
    public boolean findInPage() {
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
    @Override
    @SuppressWarnings({"NullAway"})
    public void destroy() {
        if (mPdfSandboxHandle != null) {
            mPdfSandboxHandle.close();
            mPdfSandboxHandle = null;
        }
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
        }
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
     * @param pdfFileName The filename of the downloaded pdf document.
     */
    @Override
    public void onDownloadComplete(String pdfFilePath, String pdfFileName) {
        mTitle = pdfFileName;
        loadPdfFile(pdfFilePath);
    }

    /** Returns the filepath of the pdf document. */
    @Nullable
    @Override
    public String getFilepath() {
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
        PdfUtils.recordIsUriNull(mUri == null);
        loadPdfInternal();
    }

    @Override
    public void reload() {
        if (mUri == null) {
            return;
        }
        // Remove current fragment.
        mFragmentManager
                .beginTransaction()
                .remove(mChromePdfViewerFragment)
                .commitAllowingStateLoss();
        mFragmentManager.executePendingTransactions();

        // Create new fragment.
        mChromePdfViewerFragment = new ChromePdfViewerFragment(this);

        // Add new fragment and load document again.
        loadPdfInternal();
    }

    private void loadPdfInternal() {
        if (mUri != null) {
            if (sSkipLoadPdfForTesting) {
                mIsPdfLoaded = true;
            } else {
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
                try {
                    mChromePdfViewerFragment.setDocumentUri(mUri);
                } catch (IllegalArgumentException e) {
                    Log.e(TAG, "Load pdf fails due to invalid uri.", e);
                } finally {
                    mIsPdfLoaded = true;
                }
            }
        } else {
            // TODO(crbug.com/348712628): show some error UI when content URI is null.
            Log.e(TAG, "Uri is null.");
        }
    }

    /**
     * Returns the URI of the PDF file and grants read permission to the specified target package.
     * If the target package is null, the assistant package is set as the target.
     *
     * @param isWorkProfile Whether the current profile is a work profile.
     * @param targetPackage The package name to grant URI permission to. If null, the default
     *     assistant package is used.
     * @return The URI of the PDF file, or null if the URI is not available.
     */
    @Override
    public @Nullable Uri getFileUri(boolean isWorkProfile, @Nullable String targetPackage) {
        if (mUri == null) {
            return null;
        }

        if (targetPackage == null) {
            targetPackage = PackageUtils.getDefaultAssistantPackageName(mActivity);
            PdfUtils.recordGetAssistantPackageResult(targetPackage != null);
        }

        if (targetPackage != null) {
            mActivity.grantUriPermission(
                    targetPackage, mUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        PdfUtils.recordIsWorkProfile(isWorkProfile);
        return mUri;
    }

    @Override
    public @Nullable String requestAssistContent(String filename, boolean isWorkProfile) {
        if (mUri == null) {
            return null;
        }
        String structuredData;
        try {
            structuredData =
                    new JSONObject()
                            .put(
                                    JSON_KEY_FILE_METADATA,
                                    new JSONObject()
                                            .put(JSON_KEY_FILE_NAME, filename)
                                            .put(JSON_KEY_FILE_URI, mUri.toString())
                                            .put(JSON_KEY_MIME_TYPE, MimeTypeUtils.PDF_MIME_TYPE)
                                            .put(JSON_KEY_IS_WORK_PROFILE, isWorkProfile))
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

    @Override
    public @Nullable Uri getUri() {
        return mUri;
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }

    static void skipLoadPdfForTesting(boolean skipLoadPdfForTesting) {
        var oldValue = sSkipLoadPdfForTesting;
        sSkipLoadPdfForTesting = skipLoadPdfForTesting;
        ResettersForTesting.register(() -> sSkipLoadPdfForTesting = oldValue);
    }

    // Implementation of PdfToolbarActionsDelegate
    /**
     * Navigates to the specified page.
     *
     * @param pageIndex The 0-based index of the page to navigate to.
     */
    @Override
    public void navigateToPage(int pageIndex) {
        mChromePdfViewerFragment.scrollToPage(pageIndex);
    }

    /**
     * Sets the zoom level to a specified amount.
     *
     * @param zoomLevel The new value of the zoom.
     */
    @Override
    public void changeZoomLevel(float zoomLevel) {
        mChromePdfViewerFragment.zoomTo(zoomLevel);
    }

    // Implementation of PdfActionsDelegate

    @Override
    public void loadPdfSelectionCoordinator(PdfView pdfView) {
        mPdfSelectionCoordinator = new PdfSelectionCoordinator(mActivity, pdfView);
    }

    @Override
    public boolean onLinkClicked(Uri uri) {
        if (!PdfUtils.isInlinePdfV2Enabled()) {
            return false;
        }
        String scheme = uri.getScheme();
        if (scheme == null || !ALLOWED_LINK_SCHEMES.contains(scheme.toLowerCase(Locale.ROOT))) {
            return false;
        }
        LoadUrlParams params = new LoadUrlParams(uri.toString(), PAGE_TRANSITION_TYPE);
        params.setIsRendererInitiated(true);
        // TODO(crbug.com/484103003): Reconsider initiator origin if renderer initiated is true.
        params.setInitiatorOrigin(Origin.create(new GURL(mUrl)));
        mNativePageHost.loadUrl(params, mIsIncognito);
        return true;
    }

    @Override
    public void onDocumentLoaded(int pageCount) {
        assert mToolbarCoordinator != null;
        assert mUri != null;
        assert mTitle != null;
        mToolbarCoordinator.onDocumentLoaded(pageCount, mTitle);
    }

    @Override
    public void onViewportChanged(int pageIndex, float zoomLevel) {
        assert mToolbarCoordinator != null;
        mToolbarCoordinator.onViewportChanged(pageIndex, zoomLevel);
    }
}
