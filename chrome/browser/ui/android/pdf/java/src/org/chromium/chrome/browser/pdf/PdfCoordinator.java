// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Log;
import org.chromium.chrome.browser.fakepdf.PdfDocument;
import org.chromium.chrome.browser.fakepdf.PdfDocumentListener;
import org.chromium.chrome.browser.fakepdf.PdfDocumentRequest;
import org.chromium.chrome.browser.fakepdf.PdfViewerFragment;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

/** The class responsible for setting up PdfPage. */
public class PdfCoordinator {
    private static final String TAG = "PdfCoordinator";
    private final View mView;
    private final FragmentManager mFragmentManager;
    private int mFragmentContainerViewId;
    private String mPdfFilePath;
    private boolean mPdfIsDownloaded;
    private boolean mIsPdfLoaded;
    private PdfViewerFragment mPdfViewerFragment;
    private PdfDocument mPdfDocument;

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
        mIsPdfLoaded = false;
        mView = LayoutInflater.from(host.getContext()).inflate(R.layout.pdf_page, null);
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
        setPdfFilePath(filepath);
        setPdfIsDownloaded(isPdfDownloaded());
    }

    class ChromePdfDocumentListener implements PdfDocumentListener {
        @Override
        public void onDocumentLoaded(PdfDocument document) {
            mPdfDocument = document;
            // TODO: capture metrics
        }

        @Override
        public void onDocumentLoadFailed(RuntimeException throwable) {
            // TODO: capture metrics
        }
    }

    /** Returns the intended view for PdfPage tab. */
    View getView() {
        return mView;
    }

    boolean findInPage() {
        // TODO: Invoke PdfDocument.setIsSearchVisible.
        // if (mPdfDocument != null) {
        //     mPdfDocument.setIsSearchVisible(true);
        //     return true;
        // }
        return false;
    }

    void destroy() {
        // TODO: stop download if still in progress.
        // TODO: removePdfEventsListener
    }

    void onDownloadComplete(String pdfFilePath) {
        setPdfFilePath(pdfFilePath);
        setPdfIsDownloaded(true);
    }

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
        PdfDocumentRequest pdfDocumentRequest = PdfUtils.getPdfDocumentRequest(mPdfFilePath);
        if (pdfDocumentRequest != null) {
            mPdfViewerFragment = new PdfViewerFragment();
            PdfDocumentListener documentListener = new ChromePdfDocumentListener();
            PdfUtils.loadPdf(
                    mPdfViewerFragment,
                    pdfDocumentRequest,
                    documentListener,
                    mFragmentManager,
                    mFragmentContainerViewId);
            mIsPdfLoaded = true;
        } else {
            Log.e(TAG, "PdfDocumentRequest is null.");
        }
    }

    private boolean isPdfDownloaded() {
        return mPdfFilePath != null;
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }
}
