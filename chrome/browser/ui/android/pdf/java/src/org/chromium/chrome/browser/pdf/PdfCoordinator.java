// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.net.Uri;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** The class responsible for setting up PdfPage. */
public class PdfCoordinator {
    private final View mView;
    private int mFragmentContainerViewId;
    private String mPdfFilePath;
    private boolean mPdfIsDownloaded;
    private boolean mIsPdfLoaded;

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
        setPdfFilePath(filepath);
        setPdfIsDownloaded(isPdfDownloaded(url));
    }

    /** Returns the intended view for PdfPage tab. */
    View getView() {
        return mView;
    }

    boolean findInPage() {
        // TODO: Invoke PdfViewer#setFindinfileView.
        return false;
    }

    void destroy() {
        // TODO: stop download if still in progress.
    }

    void onDownloadComplete(String pdfFilePath) {
        setPdfFilePath(pdfFilePath);
        setPdfIsDownloaded(true);
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
        // TODO: load file with PdfViewer.
        mIsPdfLoaded = true;
    }

    private boolean isPdfDownloaded(String url) {
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        assert scheme != null;
        assert scheme.equals(UrlConstants.HTTP_SCHEME)
                || scheme.equals(UrlConstants.HTTPS_SCHEME)
                || scheme.equals(UrlConstants.CONTENT_SCHEME)
                || scheme.equals(UrlConstants.FILE_SCHEME);
        return scheme.equals(UrlConstants.CONTENT_SCHEME)
                || scheme.equals(UrlConstants.FILE_SCHEME);
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }
}
