// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.net.Uri;
import android.text.Spanned;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.Set;

/** Encapsulates all data that is necessary for the URL bar to display its contents. */
@NullMarked
public class UrlBarData {
    /** The URL schemes that don't need to be displayed complete with path. */
    public static final Set<String> SCHEMES_TO_SPLIT =
            Set.of(
                    UrlConstants.BLOB_SCHEME,
                    UrlConstants.DISTILLER_SCHEME,
                    UrlConstants.HTTP_SCHEME,
                    UrlConstants.HTTPS_SCHEME);

    /**
     * URI schemes that ContentView can handle.
     *
     * <p>Copied from UrlUtilities.java. UrlUtilities uses a URI to check for schemes, which is more
     * strict than Uri and causes the path stripping to fail.
     *
     * <p>The following additions have been made: "chrome", "ftp".
     */
    private static final Set<String> ACCEPTED_SCHEMES =
            Set.of(
                    ContentUrlConstants.ABOUT_SCHEME,
                    UrlConstants.CHROME_SCHEME,
                    UrlConstants.DATA_SCHEME,
                    UrlConstants.DISTILLER_SCHEME,
                    UrlConstants.FILE_SCHEME,
                    UrlConstants.FTP_SCHEME,
                    UrlConstants.HTTP_SCHEME,
                    UrlConstants.HTTPS_SCHEME,
                    UrlConstants.INLINE_SCHEME,
                    UrlConstants.JAVASCRIPT_SCHEME);

    /** Represents an empty URL bar. */
    public static final UrlBarData EMPTY = create(null, "", 0, 0, null);

    public static UrlBarData forUrl(GURL url) {
        return forUrlAndText(url, null, null);
    }

    public static UrlBarData forNonUrlText(@Nullable CharSequence displayText) {
        return forNonUrlText(displayText, null);
    }

    public static UrlBarData forNonUrlText(
            @Nullable CharSequence displayText, @Nullable String editingText) {
        if (TextUtils.isEmpty(displayText)) {
            assert TextUtils.isEmpty(editingText) : "Attempt to create invalid UrlBarData.";
            return EMPTY;
        }
        return create(null, displayText, 0, 0, editingText);
    }

    public static UrlBarData forUrlAndText(GURL url, @Nullable CharSequence displayText) {
        return forUrlAndText(url, displayText, null);
    }

    /** Returns whether supplied URL should be shown in the Omnibox/Suggestions list. */
    public static boolean shouldShowUrl(GURL gurl, boolean isOffTheRecord) {
        return !NativePage.isChromePageUrl(gurl, isOffTheRecord) && !UrlUtilities.isNtpUrl(gurl);
    }

    /**
     * Create a new instance of UrlBarData from supplied arguments.
     *
     * @param url the canonical URL of the page, or null, if empty UrlBarData is requested
     * @param displayText the text that should be shown in the URL bar; this can be a {@link
     *     Spanned} that contains formatting to highlight parts of the display text
     * @param editingText the text that should replace the display text when editing the contents of
     *     the URL bar, or null to use the {@link #displayText} when editing
     * @returns new instance of UrlBarData, possibly UrlBarData.EMPTY if all supplied parameters are
     *     null, empty, or invalid.
     */
    public static UrlBarData forUrlAndText(
            GURL url, @Nullable CharSequence displayText, @Nullable String editingText) {
        if (GURL.isEmptyOrInvalid(url)) {
            return forNonUrlText(displayText, editingText);
        }

        // A valid URL should never be paired with an empty displayText; this likely represents an
        // error and a security risk.
        if (TextUtils.isEmpty(displayText)) {
            displayText = url.getSpec();
        }

        String displayTextStr = displayText == null ? "" : displayText.toString();

        // The displayText may come with scheme stripped. In these cases, attempting to extract
        // scheme (e.g. via Uri.parse()) may return
        // - hostname, if the address includes port number
        //   e.g. "localhost:1234" would report scheme "localhost"
        // - path fragment, if a scheme delimiter is found later
        //   e.g. "abc.com/https://def.org" would report scheme "abc.com/https"
        // Instead of attempting to extract scheme from already modified URL, verify if the
        // displayText begins with what we know to be the current URL scheme.
        String scheme = url.getScheme();
        int pathSearchOffset = 0;
        if (!scheme.isEmpty() && displayTextStr.startsWith(scheme + ":")) {
            if (!SCHEMES_TO_SPLIT.contains(scheme)) {
                return create(url, displayTextStr, 0, displayTextStr.length(), editingText);
            }

            if (UrlConstants.BLOB_SCHEME.equals(scheme)) {
                int innerSchemeSearchOffset =
                        findFirstIndexAfterSchemeSeparator(displayTextStr, scheme.length());
                Uri innerUri = Uri.parse(displayTextStr.substring(innerSchemeSearchOffset));
                String innerScheme = innerUri.getScheme();
                // Substitute the scheme to allow for proper display of end of inner origin.
                if (!TextUtils.isEmpty(innerScheme)) {
                    scheme = innerScheme;
                }
            }

            if (ACCEPTED_SCHEMES.contains(scheme)) {
                pathSearchOffset =
                        findFirstIndexAfterSchemeSeparator(
                                displayTextStr, displayTextStr.indexOf(scheme) + scheme.length());
            }
        }
        int pathOffset = -1;
        if (pathSearchOffset < displayTextStr.length()) {
            pathOffset = displayTextStr.indexOf('/', pathSearchOffset);
        }
        if (pathOffset == -1) {
            return create(url, displayText, 0, displayTextStr.length(), editingText);
        }

        // If the '/' is the last character and the beginning of the path, then just drop
        // the path entirely.
        if (pathOffset == displayText.length() - 1) {
            return create(url, displayText.subSequence(0, pathOffset), 0, pathOffset, editingText);
        }

        return create(url, displayText, 0, pathOffset, editingText);
    }

    public static UrlBarData create(
            @Nullable GURL url,
            CharSequence displayText,
            int originStartIndex,
            int originEndIndex,
            @Nullable String editingText) {
        return new UrlBarData(url, displayText, originStartIndex, originEndIndex, editingText);
    }

    private static int findFirstIndexAfterSchemeSeparator(
            CharSequence input, int searchStartIndex) {
        for (int index = searchStartIndex; index < input.length(); index++) {
            char c = input.charAt(index);
            if (c != ':' && c != '/') return index;
        }
        return input.length();
    }

    /** The canonical URL that is shown in the URL bar. */
    public final @Nullable GURL url;

    /**
     * The text that should be shown in the URL bar. This can be a {@link Spanned} that contains
     * formatting to highlight parts of the display text.
     */
    public final CharSequence displayText;

    /**
     * The text that should replace the display text when editing the contents of the URL bar, or
     * null to use the {@link #displayText} when editing.
     */
    public final @Nullable String editingText;

    /**
     * The character index in {@link #displayText} where the origin starts. This is required to
     * ensure that the end of the origin is not scrolled out of view for long hostnames.
     */
    public final int originStartIndex;

    /**
     * The character index in {@link #displayText} where the origin ends. This is required to ensure
     * that the end of the origin is not scrolled out of view for long hostnames.
     */
    public final int originEndIndex;

    /**
     * @return The text for editing, falling back to the display text if the former is null.
     */
    public CharSequence getEditingOrDisplayText() {
        return editingText != null ? editingText : displayText;
    }

    private UrlBarData(
            @Nullable GURL url,
            CharSequence displayText,
            int originStartIndex,
            int originEndIndex,
            @Nullable String editingText) {
        this.url = url;
        this.displayText = displayText;
        this.originStartIndex = originStartIndex;
        this.originEndIndex = originEndIndex;
        this.editingText = editingText;
    }
}
