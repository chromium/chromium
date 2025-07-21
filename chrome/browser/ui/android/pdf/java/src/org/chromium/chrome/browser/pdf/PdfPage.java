// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.net.Uri;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Native page that displays pdf file. */
@NullMarked
public class PdfPage extends BasicNativePage {
    @VisibleForTesting public final PdfCoordinator mPdfCoordinator;
    private String mTitle;
    private final String mUrl;
    private final boolean mIsIncognito;
    private boolean mIsDownloadSafe;
    private long mTransientDownloadStartTimestamp;

    /**
     * Create a new instance of the pdf page.
     *
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param url The pdf url, which could be a pdf link, content uri or file uri.
     * @param pdfInfo Information of the pdf.
     * @param defaultTitle Default title of the pdf page.
     * @param tabId The id of the tab.
     */
    public PdfPage(
            NativePageHost host,
            Profile profile,
            Activity activity,
            String url,
            PdfInfo pdfInfo,
            String defaultTitle,
            int tabId) {
        super(host);

        mIsDownloadSafe = pdfInfo.isDownloadSafe;
        String decodedUrl = PdfUtils.decodePdfPageUrl(url);
        String filepath =
                pdfInfo.filepath == null
                        ? PdfUtils.getFilePathFromUrl(decodedUrl)
                        : pdfInfo.filepath;
        mTitle =
                pdfInfo.filename == null
                        ? PdfUtils.getFileNameFromUrl(decodedUrl, defaultTitle)
                        : pdfInfo.filename;
        mUrl = url;
        mPdfCoordinator = new PdfCoordinator(profile, activity, filepath, tabId);
        mIsIncognito = profile.isOffTheRecord();
        initWithView(mPdfCoordinator.getView());
        // PDF is downloading when the filepath is null.
        if (filepath == null) {
            mTransientDownloadStartTimestamp = SystemClock.elapsedRealtime();
        }
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
    public @Nullable String getCanonicalFilepath() {
        return mPdfCoordinator.getFilepath();
    }

    @Override
    public boolean isDownloadSafe() {
        return mIsDownloadSafe;
    }

    @Override
    public void destroy() {
        super.destroy();
        // TODO(b/348701300): check if pdf should be opened inline.
        if (mIsIncognito) {
            PdfContentProvider.removeContentUri(mPdfCoordinator.getFilepath());
        }
        mPdfCoordinator.destroy();
    }

    /**
     * Called after pdf download complete.
     *
     * @param pdfFileName The filename of the downloaded pdf document.
     * @param pdfFilePath The filepath of the downloaded pdf document.
     * @param isDownloadSafe Whether the pdf download is safe. Mixed-content download is considered
     *     unsafe.
     */
    public void onDownloadComplete(String pdfFileName, String pdfFilePath, boolean isDownloadSafe) {
        mTitle = pdfFileName;
        mIsDownloadSafe = isDownloadSafe;
        PdfUtils.recordPdfTransientDownloadTime(
                SystemClock.elapsedRealtime() - mTransientDownloadStartTimestamp);
        // TODO(b/348701300): check if pdf should be opened inline.
        if (mIsIncognito) {
            Uri uri = PdfContentProvider.createContentUri(pdfFilePath, pdfFileName);
            if (uri == null) {
                // TODO(b/348712628): show some error UI when content URI is null.
                return;
            }
            pdfFilePath = uri.toString();
        }
        mPdfCoordinator.onDownloadComplete(pdfFilePath);
    }

    /**
     * Show pdf specific find in page UI.
     *
     * @return whether the pdf specific find in page UI is shown.
     */
    public boolean findInPage() {
        return mPdfCoordinator.findInPage();
    }

    /**
     * Retrieve uri of the pdf document.
     *
     * @return Uri of the pdf document. The uri might be null if the pdf is downloading.
     */
    public @Nullable Uri getUri() {
        return mPdfCoordinator.getUri();
    }

    /**
     * Build structured data including content uri and grant permission.
     *
     * @param isWorkProfile Whether Chrome is running in the Android work profile.
     */
    public @Nullable String requestAssistContent(boolean isWorkProfile) {
        return mPdfCoordinator.requestAssistContent(getTitle(), isWorkProfile);
    }
}
