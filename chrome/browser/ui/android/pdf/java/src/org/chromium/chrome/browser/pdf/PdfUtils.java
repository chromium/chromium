// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.os.BuildCompat;
import androidx.fragment.app.FragmentManager;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.fakepdf.PdfDocumentListener;
import org.chromium.chrome.browser.fakepdf.PdfDocumentRequest;
import org.chromium.chrome.browser.fakepdf.PdfViewSettings;
import org.chromium.chrome.browser.fakepdf.PdfViewerFragment;
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
import java.util.Locale;
import java.util.Objects;

/** Utilities for inline pdf support. */
public class PdfUtils {
    @IntDef({PdfPageType.NONE, PdfPageType.TRANSIENT, PdfPageType.LOCAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PdfPageType {
        int NONE = 0;
        int TRANSIENT = 1;
        int LOCAL = 2;
    }

    private static final String TAG = "PdfUtils";
    private static final String PDF_EXTENSION = "pdf";
    private static boolean sShouldOpenPdfInlineForTesting;
    private static boolean sSkipLoadPdfForTesting;

    /**
     * Determines whether the navigation is to a pdf file.
     *
     * @param url The url of the navigation.
     * @param params The LoadUrlParams which might be null.
     * @return Whether the navigation is to a pdf file.
     */
    public static boolean isPdfNavigation(String url, @Nullable LoadUrlParams params) {
        if (!shouldOpenPdfInline()) {
            return false;
        }
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        if (scheme == null) {
            return false;
        }
        if (scheme.equals(UrlConstants.FILE_SCHEME)) {
            // TODO(shuyng): ask the download subsystem for MIME type.
            String fileExtension = MimeTypeMap.getFileExtensionFromUrl(url).toLowerCase(Locale.US);
            return !TextUtils.isEmpty(fileExtension) && fileExtension.equals(PDF_EXTENSION);
        }
        if (scheme.equals(UrlConstants.CONTENT_SCHEME)) {
            String type = ContentUriUtils.getMimeType(url);
            return type != null && type.equals(MimeTypeUtils.PDF_MIME_TYPE);
        }
        if (scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return params != null && params.getIsPdf();
        }
        return false;
    }

    /** Determines whether to open pdf inline. */
    @CalledByNative
    public static boolean shouldOpenPdfInline() {
        if (sShouldOpenPdfInlineForTesting) return true;
        // TODO(https://crbug.com/327492784): Update checks once release plan is finalized.
        return ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_OPEN_PDF_INLINE)
                && BuildCompat.isAtLeastV();
    }

    public static PdfInfo getPdfInfo(NativePage nativePage) {
        if (nativePage == null || !nativePage.isPdf()) {
            return null;
        }
        return new PdfInfo(nativePage.getTitle(), nativePage.getCanonicalFilepath());
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
        if (getPdfPageType(gurl) == PdfPageType.LOCAL) {
            return url;
        }
        return null;
    }

    /** Return the type of the pdf page. */
    public static @PdfPageType int getPdfPageType(GURL url) {
        String scheme = url.getScheme();
        if (scheme == null) {
            return PdfPageType.NONE;
        }
        if (scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return PdfPageType.TRANSIENT;
        }
        if (scheme.equals(UrlConstants.CONTENT_SCHEME) || scheme.equals(UrlConstants.FILE_SCHEME)) {
            return PdfPageType.LOCAL;
        }
        return PdfPageType.NONE;
    }

    static void setShouldOpenPdfInlineForTesting(boolean shouldOpenPdfInlineForTesting) {
        sShouldOpenPdfInlineForTesting = shouldOpenPdfInlineForTesting;
    }

    static PdfDocumentRequest getPdfDocumentRequest(String pdfFilePath, boolean isIncognito) {
        Uri uri = Uri.parse(pdfFilePath);
        String scheme = uri.getScheme();
        PdfDocumentRequest.Builder builder = new PdfDocumentRequest.Builder();
        try {
            if (UrlConstants.CONTENT_SCHEME.equals(scheme)) {
                builder.setUri(uri);
            } else if (UrlConstants.FILE_SCHEME.equals(scheme)) {
                File file = new File(Objects.requireNonNull(uri.getPath()));
                builder.setFile(file);
            } else if (isIncognito) {
                int fd = getFileDescriptor(pdfFilePath);
                ParcelFileDescriptor pfd = ParcelFileDescriptor.adoptFd(fd);
                builder.setPfd(pfd);
            } else {
                File file = new File(pdfFilePath);
                // TODO: use builder.setFile(file) once supported.
                Uri generatedUri = ChromeFileProvider.generateUri(file);
                builder.setUri(generatedUri);
            }
        } catch (Exception e) {
            Log.e(TAG, "Couldn't generate PdfDocumentRequest: " + e);
            return null;
        }
        builder.setPdfViewSettings(
                new PdfViewSettings(/* overrideDefaultUrlClickBehavior= */ true));
        return new PdfDocumentRequest(builder);
    }

    private static int getFileDescriptor(String filepath) throws NumberFormatException {
        String fd = filepath.substring(filepath.lastIndexOf('/') + 1);
        return Integer.parseInt(fd);
    }

    static void loadPdf(
            PdfViewerFragment pdfViewerFragment,
            PdfDocumentRequest pdfDocumentRequest,
            PdfDocumentListener pdfDocumentListener,
            FragmentManager fragmentManager,
            int fragmentContainerViewId) {
        if (sSkipLoadPdfForTesting) {
            return;
        }
        // ProjectorContext.installProjectorGlobalsForTest(ContextUtils.getApplicationContext());
        pdfViewerFragment.show(
                fragmentManager, String.valueOf(fragmentContainerViewId), fragmentContainerViewId);
        pdfViewerFragment.loadRequest(pdfDocumentRequest, pdfDocumentListener);
        // TODO: pdfViewerFragment.addPdfEventsListener(eventsListener);
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

    static void skipLoadPdfForTesting(boolean skipLoadPdfForTesting) {
        sSkipLoadPdfForTesting = skipLoadPdfForTesting;
    }
}
