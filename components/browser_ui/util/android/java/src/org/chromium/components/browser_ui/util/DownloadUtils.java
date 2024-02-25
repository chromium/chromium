// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.app.DownloadManager;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** A class containing some utility static methods. */
public class DownloadUtils {
    public static final long INVALID_SYSTEM_DOWNLOAD_ID = -1;
    private static final int[] BYTES_STRINGS = {
        R.string.download_ui_kb, R.string.download_ui_mb, R.string.download_ui_gb
    };

    // Limit the origin length so that the eTLD+1 cannot be hidden. If the origin exceeds this
    // length the eTLD+1 is extracted and shown.
    public static final int MAX_ORIGIN_LENGTH_FOR_NOTIFICATION = 40;
    public static final int MAX_ORIGIN_LENGTH_FOR_DOWNLOAD_HOME_CAPTION = 25;

    /**
     * Format the number of bytes into KB, MB, or GB and return the corresponding generated string.
     * @param context Context to use.
     * @param bytes   Number of bytes needed to display.
     * @return        The formatted string to be displayed.
     */
    public static String getStringForBytes(Context context, long bytes) {
        return getStringForBytes(context, BYTES_STRINGS, bytes);
    }

    /**
     * Format the number of bytes into KB, or MB, or GB and return the corresponding string
     * resource.
     * @param context Context to use.
     * @param stringSet The string resources for displaying bytes in KB, MB and GB.
     * @param bytes Number of bytes.
     * @return A formatted string to be displayed.
     */
    public static String getStringForBytes(Context context, int[] stringSet, long bytes) {
        int resourceId;
        float bytesInCorrectUnits;

        if (ConversionUtils.bytesToMegabytes(bytes) < 1) {
            resourceId = stringSet[0];
            bytesInCorrectUnits = bytes / (float) ConversionUtils.BYTES_PER_KILOBYTE;
        } else if (ConversionUtils.bytesToGigabytes(bytes) < 1) {
            resourceId = stringSet[1];
            bytesInCorrectUnits = bytes / (float) ConversionUtils.BYTES_PER_MEGABYTE;
        } else {
            resourceId = stringSet[2];
            bytesInCorrectUnits = bytes / (float) ConversionUtils.BYTES_PER_GIGABYTE;
        }

        return context.getResources().getString(resourceId, bytesInCorrectUnits);
    }

    /**
     * Adds a download to the Android DownloadManager.
     * @see android.app.DownloadManager#addCompletedDownload(String, String, boolean, String,
     * String, long, boolean)
     */
    public static long addCompletedDownload(
            String fileName,
            String description,
            String mimeType,
            String filePath,
            long fileSizeBytes,
            GURL originalUrl,
            GURL referer) {
        assert !ThreadUtils.runningOnUiThread();
        assert Build.VERSION.SDK_INT < Build.VERSION_CODES.Q
                : "addCompletedDownload is deprecated in Q, may cause crash.";
        Context context = ContextUtils.getApplicationContext();
        DownloadManager manager =
                (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        boolean useSystemNotification = !notificationManager.areNotificationsEnabled();
        try {
            // OriginalUri has to be null or non-empty http(s) scheme.
            Uri originalUri = parseOriginalUrl(originalUrl.getSpec());
            Uri refererUri = GURL.isEmptyOrInvalid(referer) ? null : Uri.parse(referer.getSpec());
            return manager.addCompletedDownload(
                    fileName,
                    description,
                    true,
                    mimeType,
                    filePath,
                    fileSizeBytes,
                    useSystemNotification,
                    originalUri,
                    refererUri);
        } catch (Exception e) {
            return INVALID_SYSTEM_DOWNLOAD_ID;
        }
    }

    /**
     * Parses an originating URL string and returns a valid Uri that can be inserted into
     * DownloadManager. The returned Uri has to be null or non-empty http(s) scheme.
     * @param originalUrl String representation of the originating URL.
     * @return A valid Uri that can be accepted by DownloadManager.
     */
    public static Uri parseOriginalUrl(String originalUrl) {
        Uri originalUri = TextUtils.isEmpty(originalUrl) ? null : Uri.parse(originalUrl);
        if (originalUri != null) {
            String scheme = originalUri.normalizeScheme().getScheme();
            if (scheme == null
                    || (!scheme.equals(UrlConstants.HTTPS_SCHEME)
                            && !scheme.equals(UrlConstants.HTTP_SCHEME))) {
                originalUri = null;
            }
        }
        return originalUri;
    }

    /**
     * Adjusts a URL for display to the user in a text view subject to char limits. Could elide
     * parts the URL if it is too long as per readability and security aspects.
     *
     * @param url The full URL.
     * @param limit Character limit.
     * @return The text to display, or null if the input was invalid.
     */
    public static String formatUrlForDisplayInNotification(GURL url, int limit) {
        if (GURL.isEmptyOrInvalid(url)) return null;

        String formattedUrl =
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        if (formattedUrl.length() <= limit) return formattedUrl;

        // The origin is too long. Strip down to eTLD+1.
        return UrlUtilities.getDomainAndRegistry(
                url.getSpec(), /* includePrivateRegistries= */ false);
    }
}
