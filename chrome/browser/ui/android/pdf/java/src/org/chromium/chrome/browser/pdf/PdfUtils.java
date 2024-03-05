// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import androidx.annotation.Nullable;

import org.chromium.base.ContentUriUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.File;
import java.util.Locale;

/** Utilities for inline pdf support. */
public class PdfUtils {
    // TODO(shuyng): add this to android_chrome_strings.grd once the UX spec is finalized.
    static final String PDF_PAGE_TITLE = "PDF";
    private static final String PDF_EXTENSION = "pdf";
    private static boolean sUseAndroidPdfViewerForTesting;

    /**
     * Determines whether the navigation is to a pdf file.
     *
     * @param url The url of the navigation.
     * @param params The LoadUrlParams which might be null.
     * @return Whether the navigation is to a pdf file.
     */
    public static boolean isPdfNavigation(String url, @Nullable LoadUrlParams params) {
        if (!useAndroidPdfViewer()) {
            return false;
        }
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        assert scheme != null;
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

    /** Determines whether to use the Android PdfViewer to render pdf inline. */
    public static boolean useAndroidPdfViewer() {
        if (sUseAndroidPdfViewerForTesting) return true;
        // TODO(https://crbug.com/327492784): Update checks once release plan is finalized.
        return ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_OPEN_PDF_INLINE)
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
    }

    static String getFileNameFromUrl(String url) {
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        assert scheme != null;
        assert scheme.equals(UrlConstants.HTTP_SCHEME)
                || scheme.equals(UrlConstants.HTTPS_SCHEME)
                || scheme.equals(UrlConstants.CONTENT_SCHEME)
                || scheme.equals(UrlConstants.FILE_SCHEME);
        String fileName = PDF_PAGE_TITLE;
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
        Uri uri = Uri.parse(url);
        String scheme = uri.getScheme();
        assert scheme != null;
        assert scheme.equals(UrlConstants.HTTP_SCHEME)
                || scheme.equals(UrlConstants.HTTPS_SCHEME)
                || scheme.equals(UrlConstants.CONTENT_SCHEME)
                || scheme.equals(UrlConstants.FILE_SCHEME);
        if (scheme.equals(UrlConstants.CONTENT_SCHEME) || scheme.equals(UrlConstants.FILE_SCHEME)) {
            return url;
        }
        return "";
    }

    static void setUseAndroidPdfViewerForTesting(boolean useAndroidPdfViewerForTesting) {
        sUseAndroidPdfViewerForTesting = useAndroidPdfViewerForTesting;
    }
}
