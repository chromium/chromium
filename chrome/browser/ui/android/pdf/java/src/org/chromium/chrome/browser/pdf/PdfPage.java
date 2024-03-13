// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Native page that displays pdf file. */
public class PdfPage extends BasicNativePage {
    @VisibleForTesting final PdfCoordinator mPdfCoordinator;
    private String mTitle;
    private final String mUrl;

    /**
     * Create a new instance of the pdf page.
     *
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param url The pdf url, which could be a pdf link, content uri or file uri.
     */
    public PdfPage(NativePageHost host, Profile profile, Activity activity, String url) {
        super(host);

        String filepath = PdfUtils.getFilePathFromUrl(url);
        mTitle = PdfUtils.getFileNameFromUrl(url);
        mUrl = url;
        mPdfCoordinator = new PdfCoordinator(host, profile, activity, filepath, url);
        initWithView(mPdfCoordinator.getView());
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getUrl() {
        return mUrl;
    }

    @Override
    public String getHost() {
        return UrlConstants.PDF_HOST;
    }

    @Override
    public boolean isPdf() {
        return true;
    }

    @Override
    public void destroy() {
        super.destroy();
        mPdfCoordinator.destroy();
    }

    public void onDownloadComplete(String pdfFileName, String pdfFilePath) {
        mTitle = pdfFileName;
        mPdfCoordinator.onDownloadComplete(pdfFilePath);
    }

    public boolean findInPage() {
        return mPdfCoordinator.findInPage();
    }
}
