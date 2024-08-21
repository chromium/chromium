// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.net.Uri;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.pdf.viewer.fragment.PdfViewerFragment;

import org.chromium.base.Log;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** The class responsible for setting up PdfPage. */
public class PdfCoordinator {
    private static final String TAG = "PdfCoordinator";
    private NativePageHost mHost;
    private final View mView;
    private final FragmentManager mFragmentManager;
    private int mFragmentContainerViewId;
    private String mPdfFilePath;
    private boolean mPdfIsDownloaded;
    private boolean mIsPdfLoaded;
    private ChromePdfViewerFragment mChromePdfViewerFragment;

    /**
     * Creates a PdfCoordinator for the PdfPage.
     *
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param filepath The pdf filepath.
     * @param url The pdf url, which could a pdf link, content uri or file uri.
     */
    public PdfCoordinator(
            NativePageHost host, Profile profile, Activity activity, String filepath, String url) {
        mHost = host;
        mView = LayoutInflater.from(host.getContext()).inflate(R.layout.pdf_page, null);
        mView.setBackgroundColor(
                ChromeColors.getPrimaryBackgroundColor(
                        host.getContext(), profile.isOffTheRecord()));
        mView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        loadPdfFileIfNeeded();
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });
        View fragmentContainerView = mView.findViewById(R.id.pdf_fragment_container);
        mFragmentContainerViewId = View.generateViewId();
        fragmentContainerView.setId(mFragmentContainerViewId);
        mFragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        // Create PdfViewerFragment to start showing the loading spinner.
        mChromePdfViewerFragment = new ChromePdfViewerFragment();
        setPdfFilePath(filepath);
        setPdfIsDownloaded(isPdfDownloaded());
    }

    /** The class responsible for rendering pdf document. */
    public static class ChromePdfViewerFragment extends PdfViewerFragment {
        boolean mIsLoadDocumentSuccess;

        @Override
        public void onLoadDocumentSuccess() {
            mIsLoadDocumentSuccess = true;
            // TODO: capture metrics
        }

        @Override
        public void onLoadDocumentError(@NonNull Throwable throwable) {
            // TODO: capture metrics
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
            return true;
        }
        return false;
    }

    /**
     * Called after a pdf page has been removed from the view hierarchy and will no longer be used.
     */
    void destroy() {
        mChromePdfViewerFragment = null;
    }

    /**
     * Called after pdf download complete.
     *
     * @param pdfFilePath The filepath of the downloaded pdf document.
     */
    void onDownloadComplete(String pdfFilePath) {
        setPdfFilePath(pdfFilePath);
        setPdfIsDownloaded(true);
    }

    /** Returns the filepath of the pdf document. */
    String getFilepath() {
        return mPdfFilePath;
    }

    private void setPdfFilePath(String pdfFilePath) {
        mPdfFilePath = pdfFilePath;
    }

    private void setPdfIsDownloaded(boolean pdfIsDownloaded) {
        mPdfIsDownloaded = pdfIsDownloaded;
        loadPdfFileIfNeeded();
    }

    private void loadPdfFileIfNeeded() {
        if (mIsPdfLoaded) {
            return;
        }
        if (!mPdfIsDownloaded) {
            return;
        }
        if (mView.getParent() == null) {
            return;
        }
        Uri uri = PdfUtils.getUriFromFilePath(mPdfFilePath);
        if (uri != null) {
            PdfUtils.loadPdf(
                    mChromePdfViewerFragment, uri, mFragmentManager, mFragmentContainerViewId);
            mIsPdfLoaded = true;
        } else {
            Log.e(TAG, "Uri is null.");
        }
    }

    private boolean isPdfDownloaded() {
        return mPdfFilePath != null;
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }
}
