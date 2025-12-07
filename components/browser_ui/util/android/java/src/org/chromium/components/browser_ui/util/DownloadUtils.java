// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.Context;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.download.DownloadDangerType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** A class containing some utility static methods. */
@NullMarked
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

        return context.getString(resourceId, bytesInCorrectUnits);
    }

    /**
     * Parses an originating URL string and returns a valid Uri that can be inserted into
     * DownloadManager. The returned Uri has to be null or non-empty http(s) scheme.
     *
     * @param originalUrl String representation of the originating URL.
     * @return A valid Uri that can be accepted by DownloadManager.
     */
    public static @Nullable Uri parseOriginalUrl(String originalUrl) {
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
     * @return The text to display, or null if the input was invalid or cannot be shortened enough.
     */
    public static @Nullable String formatUrlForDisplayInNotification(
            @Nullable GURL url, int limit) {
        if (GURL.isEmptyOrInvalid(url)) return null;

        String formattedUrl =
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        if (!TextUtils.isEmpty(formattedUrl) && formattedUrl.length() <= limit) {
            return formattedUrl;
        }

        // The formatted URL is unsuitable. One possible fallback is eTLD+1, but we should be
        // careful to only parse for eTLD+1 if the origin has a host portion (some URL schemes
        // don't).
        GURL origin = url.getOrigin();
        String fallback =
                !GURL.isEmptyOrInvalid(origin) && !origin.getHost().isEmpty()
                        ? UrlUtilities.getDomainAndRegistry(
                                origin.getSpec(), /* includePrivateRegistries= */ true)
                        : origin.getPossiblyInvalidSpec();
        if (!TextUtils.isEmpty(fallback) && fallback.length() <= limit) {
            return fallback;
        }
        return null;
    }

    /**
     * @return Whether a download should be displayed as "dangerous" throughout the Android download
     *     UI. Used for items with Safe Browsing download warnings.
     */
    public static boolean shouldDisplayDownloadAsDangerous(
            @DownloadDangerType int dangerType, @OfflineItemState int state) {
        // TODO(crbug.com/397407934): These are the only danger types which we currently choose to
        // show warning UI for. In the future, this may or may not expand to other danger types.
        // Note that this is a stricter subset of danger types than we count as
        // {@link OfflineItem#isDangerous}.
        boolean dangerTypeShouldDisplayAsDangerous =
                dangerType == DownloadDangerType.DANGEROUS_CONTENT
                        || dangerType == DownloadDangerType.POTENTIALLY_UNWANTED;
        return dangerTypeShouldDisplayAsDangerous && state != OfflineItemState.CANCELLED;
    }
}
