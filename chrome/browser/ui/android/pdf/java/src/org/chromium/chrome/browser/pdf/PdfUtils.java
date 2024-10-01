// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.os.BuildCompat;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.util.Objects;

/** Utilities for inline pdf support. */
public class PdfUtils {
    @IntDef({
        PdfPageType.NONE,
        PdfPageType.TRANSIENT_SECURE,
        PdfPageType.LOCAL,
        PdfPageType.TRANSIENT_INSECURE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PdfPageType {
        int NONE = 0;
        int TRANSIENT_SECURE = 1;
        int LOCAL = 2;
        int TRANSIENT_INSECURE = 3;
    }

    private static final String TAG = "PdfUtils";
    private static final String PARAM_ANDROID_INLINE_PDF_IN_INCOGNITO = "inline_pdf_in_incognito";
    private static boolean sShouldOpenPdfInlineForTesting;

    /**
     * Determines whether the navigation is to a pdf file.
     *
     * @param url The url of the navigation.
     * @param params The LoadUrlParams which might be null.
     * @return Whether the navigation is to a pdf file.
     */
    public static boolean isPdfNavigation(String url, @Nullable LoadUrlParams params) {
        String scheme = getDecodedScheme(url);
        if (scheme == null) {
            return false;
        }

        if (scheme.equals(UrlConstants.FILE_SCHEME) || scheme.equals(UrlConstants.CONTENT_SCHEME)) {
            return true;
        }
        if (scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return params != null && params.getIsPdf();
        }
        return false;
    }

    /**
     * Determines whether the navigation is to a permanent downloaded pdf file.
     *
     * @param url The url of the navigation.
     * @return Whether the navigation is to a permanent downloaded pdf file.
     */
    public static boolean isDownloadedPdf(String url) {
        String scheme = getDecodedScheme(url);
        if (scheme == null) {
            return false;
        }

        return scheme.equals(UrlConstants.FILE_SCHEME)
                || scheme.equals(UrlConstants.CONTENT_SCHEME);
    }

    private static String getDecodedScheme(String url) {
        String decodedUrl = PdfUtils.decodePdfPageUrl(url);
        if (decodedUrl == null) {
            return null;
        }
        Uri uri = Uri.parse(decodedUrl);
        return uri.getScheme();
    }

    /**
     * Determines whether to open pdf inline.
     *
     * @param isIncognito Whether the current page is in an incognito mode.
     * @return Whether to open pdf inline.
     */
    @CalledByNative
    public static boolean shouldOpenPdfInline(boolean isIncognito) {
        if (sShouldOpenPdfInlineForTesting) return true;
        if (!ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_OPEN_PDF_INLINE)) {
            return false;
        }
        if (isIncognito
                && !ContentFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsBoolean(
                                ContentFeatureList.ANDROID_OPEN_PDF_INLINE,
                                PARAM_ANDROID_INLINE_PDF_IN_INCOGNITO,
                                false)) {
            return false;
        }
        // TODO(https://crbug.com/337674493): Check if pdf viewer is available on pre-V devices.
        return BuildCompat.isAtLeastV();
    }

    /**
     * Retrieve pdf specific information from NativePage.
     *
     * @param nativePage The NativePage being used to retrieve pdf information.
     * @return Pdf information including filename, filepath etc.
     */
    public static PdfInfo getPdfInfo(NativePage nativePage) {
        if (nativePage == null || !nativePage.isPdf()) {
            return null;
        }
        return new PdfInfo(
                nativePage.getTitle(),
                nativePage.getCanonicalFilepath(),
                nativePage.isDownloadSafe());
    }

    static String getFileNameFromUrl(String url, String defaultTitle) {
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        assert scheme != null;
        assert scheme.equals(UrlConstants.HTTP_SCHEME)
                || scheme.equals(UrlConstants.HTTPS_SCHEME)
                || scheme.equals(UrlConstants.CONTENT_SCHEME)
                || scheme.equals(UrlConstants.FILE_SCHEME);
        String fileName = defaultTitle;
        if (scheme.equals(UrlConstants.CONTENT_SCHEME)) {
            String displayName = ContentUriUtils.maybeGetDisplayName(url);
            if (!TextUtils.isEmpty(displayName)) {
                fileName = displayName;
            }
        } else if (scheme.equals(UrlConstants.FILE_SCHEME)) {
            if (uri.getPath() != null) {
                File file = new File(uri.getPath());
                if (!file.getName().isEmpty()) {
                    fileName = file.getName();
                }
            }
        }
        return fileName;
    }

    static String getFilePathFromUrl(String url) {
        GURL gurl = new GURL(url);
        if (getPdfPageTypeInternal(gurl, false) == PdfPageType.LOCAL) {
            return url;
        }
        return null;
    }

    /** Return the type of the pdf page. */
    public static @PdfPageType int getPdfPageType(NativePage pdfPage) {
        if (pdfPage == null || !pdfPage.isPdf()) {
            return PdfPageType.NONE;
        }
        assert pdfPage instanceof PdfPage;
        GURL url = new GURL(pdfPage.getUrl());
        return getPdfPageTypeInternal(url, pdfPage.isDownloadSafe());
    }

    private static @PdfPageType int getPdfPageTypeInternal(GURL url, boolean isDownloadSafe) {
        // The url may be encoded. Try to decode first.
        String scheme = getDecodedScheme(url.getSpec());
        // Get scheme from url directly if fail to decode.
        if (scheme == null) {
            scheme = url.getScheme();
        }

        if (scheme == null) {
            return PdfPageType.NONE;
        }
        if (scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return isDownloadSafe ? PdfPageType.TRANSIENT_SECURE : PdfPageType.TRANSIENT_INSECURE;
        }
        if (scheme.equals(UrlConstants.CONTENT_SCHEME) || scheme.equals(UrlConstants.FILE_SCHEME)) {
            return PdfPageType.LOCAL;
        }
        return PdfPageType.NONE;
    }

    static void setShouldOpenPdfInlineForTesting(boolean shouldOpenPdfInlineForTesting) {
        sShouldOpenPdfInlineForTesting = shouldOpenPdfInlineForTesting;
    }

    static Uri getUriFromFilePath(@NonNull String pdfFilePath) {
        Uri uri = Uri.parse(pdfFilePath);
        String scheme = uri.getScheme();
        try {
            if (UrlConstants.CONTENT_SCHEME.equals(scheme)) {
                return uri;
            } else if (UrlConstants.FILE_SCHEME.equals(scheme)) {
                File file = new File(Objects.requireNonNull(uri.getPath()));
                return ChromeFileProvider.generateUri(file);
            } else {
                File file = new File(pdfFilePath);
                return ChromeFileProvider.generateUri(file);
            }
        } catch (Exception e) {
            Log.e(TAG, "Couldn't generate Uri: " + e);
            return null;
        }
    }

    /**
     * Record boolean histogram Android.Pdf.IsFrozenWhenDisplayed.
     *
     * @param nativePage When the native page is a pdf page, record whether it is frozen before the
     *     tab is displayed.
     */
    public static void recordIsPdfFrozen(NativePage nativePage) {
        if (nativePage == null) {
            return;
        }
        if (!nativePage.isPdf()) {
            return;
        }
        RecordHistogram.recordBooleanHistogram(
                "Android.Pdf.IsFrozenWhenDisplayed", nativePage.isFrozen());
    }

    /**
     * Encode the download url and generate the pdf page url.
     *
     * @param downloadUrl The url which is interpreted as download.
     * @return The pdf page url including the encoded downloadUrl.
     */
    public static String encodePdfPageUrl(String downloadUrl) {
        try {
            String pdfPageUrl =
                    UrlConstants.PDF_URL
                            + UrlConstants.PDF_URL_PARAM
                            + URLEncoder.encode(downloadUrl, "UTF-8");
            recordIsPdfDownloadUrlEncoded(true);
            return pdfPageUrl;
        } catch (java.io.UnsupportedEncodingException e) {
            recordIsPdfDownloadUrlEncoded(false);
            Log.e(TAG, "Unsupported encoding: " + e.getMessage());
            return null;
        }
    }

    /**
     * Decode the pdf page url.
     *
     * @param originalUrl The url to be decoded.
     * @return the decoded download url; or null if the original url is not a pdf page url.
     */
    public static String decodePdfPageUrl(String originalUrl) {
        if (originalUrl == null || !originalUrl.startsWith(UrlConstants.PDF_URL)) {
            return null;
        }
        Uri uri = Uri.parse(originalUrl);
        String encodedUrl = uri.getQueryParameter(UrlConstants.PDF_URL_QUERY_PARAM);
        try {
            String decodedUrl = URLDecoder.decode(encodedUrl, "UTF-8");
            recordIsPdfDownloadUrlDecoded(true);
            return decodedUrl;
        } catch (Exception e) {
            recordIsPdfDownloadUrlDecoded(false);
            Log.e(TAG, "Unsupported encoding: " + e.getMessage());
            return null;
        }
    }

    static void recordPdfLoad() {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DocumentLoad", true);
    }

    static void recordPdfLoadResult(boolean isLoadSuccess) {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DocumentLoadResult", isLoadSuccess);
    }

    static void recordPdfLoadTime(long duration) {
        RecordHistogram.recordTimesHistogram("Android.Pdf.DocumentLoadTime", duration);
    }

    static void recordFindInPage(int findInPageCounts) {
        RecordHistogram.recordExactLinearHistogram(
                "Android.Pdf.FindInPageCounts", findInPageCounts, /* max= */ 9);
    }

    static void recordHasFilepathWithoutFragmentOnDestroy(boolean hasFilepath) {
        RecordHistogram.recordBooleanHistogram(
                "Android.Pdf.HasFilepathWithoutFragmentOnDestroy", hasFilepath);
    }

    private static void recordIsPdfDownloadUrlEncoded(boolean encodeResult) {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DownloadUrlEncoded", encodeResult);
    }

    private static void recordIsPdfDownloadUrlDecoded(boolean decodeResult) {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DownloadUrlDecoded", decodeResult);
    }
}
