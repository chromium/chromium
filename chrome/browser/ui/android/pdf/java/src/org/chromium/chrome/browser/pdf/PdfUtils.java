// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.ext.SdkExtensions;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.url.GURL;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URLEncoder;
import java.util.Set;

/** Utilities for inline pdf support. */
@NullMarked
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

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        PdfLoadResult.SUCCESS,
        PdfLoadResult.ERROR,
        PdfLoadResult.ABORT,
        PdfLoadResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PdfLoadResult {
        int SUCCESS = 0;
        int ERROR = 1;
        int ABORT = 2;

        int NUM_ENTRIES = 3;
    }

    private static final String TAG = "PdfUtils";
    private static final String PARAM_ANDROID_INLINE_PDF_IN_INCOGNITO = "inline_pdf_in_incognito";
    private static final String PARAM_ANDROID_INLINE_PDF_BACKPORT_IN_INCOGNITO =
            "inline_pdf_backport_in_incognito";
    private static final Set<String> TRANSIENT_PDF_SCHEMES =
            Set.of(
                    UrlConstants.HTTP_SCHEME,
                    UrlConstants.HTTPS_SCHEME,
                    UrlConstants.BLOB_SCHEME,
                    UrlConstants.DATA_SCHEME);
    private static final Set<String> PERMANENT_PDF_SCHEMES =
            Set.of(UrlConstants.CONTENT_SCHEME, UrlConstants.FILE_SCHEME);
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

        if (PERMANENT_PDF_SCHEMES.contains(scheme)) {
            return true;
        }
        if (TRANSIENT_PDF_SCHEMES.contains(scheme)) {
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

        return PERMANENT_PDF_SCHEMES.contains(scheme);
    }

    private static @Nullable String getDecodedScheme(String url) {
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
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
            return true;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && SdkExtensions.getExtensionVersion(Build.VERSION_CODES.S) >= 13) {
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_OPEN_PDF_INLINE_BACKPORT)) {
                return false;
            }
            if (isIncognito
                    && !ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.ANDROID_OPEN_PDF_INLINE_BACKPORT,
                            PARAM_ANDROID_INLINE_PDF_BACKPORT_IN_INCOGNITO,
                            false)) {
                return false;
            }
            return true;
        }
        return false;
    }

    /**
     * Retrieve pdf specific information from NativePage.
     *
     * @param nativePage The NativePage being used to retrieve pdf information.
     * @return Pdf information including filename, filepath etc.
     */
    public static @Nullable PdfInfo getPdfInfo(@Nullable NativePage nativePage) {
        if (nativePage == null || !nativePage.isPdf()) {
            return null;
        }
        return new PdfInfo(
                nativePage.getTitle(),
                nativePage.getCanonicalFilepath(),
                nativePage.isDownloadSafe());
    }

    static String getFileNameFromUrl(@Nullable String url, String defaultTitle) {
        if (url == null) {
            return defaultTitle;
        }
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        assert scheme != null;
        assert TRANSIENT_PDF_SCHEMES.contains(scheme) || PERMANENT_PDF_SCHEMES.contains(scheme);
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

    static @Nullable String getFilePathFromUrl(@Nullable String url) {
        if (url == null) {
            return null;
        }
        GURL gurl = new GURL(url);
        if (getPdfPageTypeInternal(gurl, false) == PdfPageType.LOCAL) {
            return url;
        }
        return null;
    }

    /** Return the type of the pdf page. */
    public static @PdfPageType int getPdfPageType(@Nullable NativePage pdfPage) {
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
        if (TRANSIENT_PDF_SCHEMES.contains(scheme)) {
            return isDownloadSafe ? PdfPageType.TRANSIENT_SECURE : PdfPageType.TRANSIENT_INSECURE;
        }
        if (PERMANENT_PDF_SCHEMES.contains(scheme)) {
            return PdfPageType.LOCAL;
        }
        return PdfPageType.NONE;
    }

    static void setShouldOpenPdfInlineForTesting(boolean shouldOpenPdfInlineForTesting) {
        sShouldOpenPdfInlineForTesting = shouldOpenPdfInlineForTesting;
    }

    static @Nullable Uri getUriFromFilePath(String pdfFilePath) {
        Uri uri = Uri.parse(pdfFilePath);
        String scheme = uri.getScheme();
        try {
            if (UrlConstants.CONTENT_SCHEME.equals(scheme)
                    || UrlConstants.FILE_SCHEME.equals(scheme)) {
                // PDF androidx library accepts file or content URI.
                return uri;
            } else {
                // Convert filepath to Uri for transient downloads.
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
    public static void recordIsPdfFrozen(@Nullable NativePage nativePage) {
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
    public static @Nullable String encodePdfPageUrl(String downloadUrl) {
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
    public static @Nullable String decodePdfPageUrl(String originalUrl) {
        if (originalUrl == null || !originalUrl.startsWith(UrlConstants.PDF_URL)) {
            return null;
        }
        Uri uri = Uri.parse(originalUrl);
        try {
            // #getQueryParameter has already decoded the url.
            String decodedUrl = uri.getQueryParameter(UrlConstants.PDF_URL_QUERY_PARAM);
            recordIsPdfDownloadUrlDecoded(true);
            return decodedUrl;
        } catch (Exception e) {
            recordIsPdfDownloadUrlDecoded(false);
            Log.e(TAG, "Unsupported encoding: " + e.getMessage());
            return null;
        }
    }

    /**
     * Encode content uri if it is PDF MIME type.
     *
     * @param uri The uri to be encoded.
     * @param context The {@link Context} to retrieve {@link ContentResolver}.
     * @return the encoded content uri if it is PDF MIME type; or null otherwise.
     */
    public static @Nullable String getEncodedContentUri(@Nullable String uri, Context context) {
        if (TextUtils.isEmpty(uri)
                || !UrlConstants.CONTENT_SCHEME.equals(Uri.parse(uri).getScheme())) {
            return null;
        }
        ContentResolver contentResolver = context.getContentResolver();
        String mimeType = contentResolver.getType(Uri.parse(uri));
        if (MimeTypeUtils.PDF_MIME_TYPE.equals(mimeType)) {
            return PdfUtils.encodePdfPageUrl(uri);
        }
        return null;
    }

    static void recordPdfLoad() {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DocumentLoad", true);
    }

    static void recordPdfLoadResultDetail(@PdfLoadResult int loadResult) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Pdf.DocumentLoadResult.Detail", loadResult, PdfLoadResult.NUM_ENTRIES);
    }

    static void recordPdfLoadTimeFirstPaired(long duration) {
        RecordHistogram.recordTimesHistogram("Android.Pdf.DocumentLoadTime.FirstPaired", duration);
    }

    static void recordPdfLoadInterval(long duration) {
        RecordHistogram.recordMediumTimesHistogram("Android.Pdf.DocumentLoadInterval", duration);
    }

    static void recordPdfTransientDownloadTime(long duration) {
        RecordHistogram.recordTimesHistogram("Android.Pdf.DownloadTime.Transient", duration);
    }

    static void recordFindInPage(int findInPageCounts) {
        RecordHistogram.recordExactLinearHistogram(
                "Android.Pdf.FindInPageCounts", findInPageCounts, /* max= */ 9);
    }

    static void recordIsWorkProfile(boolean isWorkProfile) {
        RecordHistogram.recordBooleanHistogram(
                "Android.Pdf.AssistContent.IsWorkProfile", isWorkProfile);
    }

    static void recordGetAssistantPackageResult(boolean success) {
        RecordHistogram.recordBooleanHistogram(
                "Android.Pdf.AssistContent.GetAssistantPackageResult", success);
    }

    private static void recordIsPdfDownloadUrlEncoded(boolean encodeResult) {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DownloadUrlEncoded", encodeResult);
    }

    private static void recordIsPdfDownloadUrlDecoded(boolean decodeResult) {
        RecordHistogram.recordBooleanHistogram("Android.Pdf.DownloadUrlDecoded", decodeResult);
    }
}
