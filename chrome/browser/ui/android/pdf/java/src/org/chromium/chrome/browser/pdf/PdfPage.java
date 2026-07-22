// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.app.Activity;
import android.net.Uri;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.modules.on_demand.OnDemandModule;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Native page that displays pdf file. */
@NullMarked
public class PdfPage extends BasicNativePage {
    @VisibleForTesting public final PdfCoordinatorInterface mPdfCoordinator;
    private String mTitle;
    private String mUrl;
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
     * @param pdfFragmentViewTracker Tracks PdfViewerFragment's View to assign to the right PdfPage.
     */
    public PdfPage(
            NativePageHost host,
            Tab tab,
            Activity activity,
            String url,
            PdfInfo pdfInfo,
            String defaultTitle,
            PdfFragmentViewTracker pdfFragmentViewTracker) {
        super(host);

        Profile profile = tab.getProfile();
        mIsIncognito = profile.isOffTheRecord();
        int tabId = tab.getId();
        if (mIsIncognito) {
            // Bind the PDF stream lifetime to the Tab instead of the transient PdfPage view.
            PdfTabHelper.from(tab).setPdfUrl(url);
        }
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
        mPdfCoordinator =
                OnDemandModule.getImpl()
                        .getPdfEntryPoint()
                        .createPdfCoordinator(
                                host,
                                profile,
                                activity,
                                url,
                                filepath,
                                mTitle,
                                tabId,
                                pdfFragmentViewTracker);
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
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        if (!PdfUtils.isReuseFragmentEnabled()) return;

        mPdfCoordinator.resetLoadState();
        mUrl = url;
        // Note that only local PDF loading is handled here. Non-local ones are taken care of
        // by DownloadController#onDownloadCompleted.
        if (!PdfUtils.isDownloadedPdf(url)) return;

        // Use the URL encoded in |mUrl| if available i.e. chrome-native://pdf/link?url=...
        String pageUrl = PdfUtils.decodePdfPageUrl(url);
        String pdfUrl = pageUrl != null ? pageUrl : url;
        mPdfCoordinator.onDownloadComplete(pdfUrl, PdfUtils.getFileNameFromUrl(pdfUrl, ""));
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
        // Stream cleanup is now managed by PdfTabHelper based on Tab lifecycle
        // to support window swapping (drag and drop) without timers.
        mPdfCoordinator.destroy();
    }

    @Override
    public void reload() {
        if (PdfUtils.isInlinePdfV2Enabled()) {
            mPdfCoordinator.reload();
        }
    }

    @Override
    public boolean shouldReusePage(@Nullable String curl, String nurl, boolean preferReuse) {
        if (!PdfUtils.isReuseFragmentEnabled()) return TextUtils.equals(curl, nurl);

        // For PDF page, we reuse NativePage by default, with 2 exceptions:
        // - When the current NativePage is frozen
        // - When the URL is reloaded on Activity restart.
        //
        // TODO(crbug.com/514819449): If downloading a non-local PDF takes longer, reusing
        //    NativePage keeps showing the old one, which could be perceived as failure in
        //    loading. Creating a new NativePage/Fragment can avoid it as it displays
        //    a spinner while download is progress.
        return !isFrozen() && !isLoadingAfterActivityRestarted(curl, nurl, preferReuse);
    }

    private static boolean isLoadingAfterActivityRestarted(
            @Nullable String curl, String nurl, boolean preferReuse) {
        if (preferReuse) return false;

        String cdurl = PdfUtils.decodePdfPageUrl(curl);
        if (PdfUtils.isDownloadedPdf(nurl) && cdurl != null) {
            // Local pdf. Both new and old one are chrome-native://pdf/link?url=content://
            String ndurl = PdfUtils.decodePdfPageUrl(nurl);
            return ndurl != null && TextUtils.equals(cdurl, ndurl);
        } else {
            // Non-local pdf
            // 1) old chrome-native://pdflink?url=encoded(URL) vs. new: URL
            // 2) old URL == new URL, not for the scheme chrome-native://pdf/link?url=http..
            //     Upon activiy restart, the reloaded URL is not an encoded form.
            return curl != null
                    && (TextUtils.equals(cdurl, nurl)
                            || (!curl.startsWith(UrlConstants.PDF_URL)
                                    && TextUtils.equals(curl, nurl)));
        }
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
        mPdfCoordinator.onDownloadComplete(pdfFilePath, pdfFileName);
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

    /**
     * Retrieve uri of the pdf document and grant permission to the target package.
     *
     * @param isWorkProfile Whether Chrome is running in the Android work profile.
     * @param targetPackage The package to grant access to. If null, the default assistant package
     *     will be used.
     */
    public @Nullable Uri getFileUri(boolean isWorkProfile, @Nullable String targetPackage) {
        return mPdfCoordinator.getFileUri(isWorkProfile, targetPackage);
    }
}
