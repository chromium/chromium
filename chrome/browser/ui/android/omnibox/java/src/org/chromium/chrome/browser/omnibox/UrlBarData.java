// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.net.Uri;
import android.text.Spanned;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.Set;

/**
 * Encapsulates all data that is necessary for the URL bar to display its contents.
 */
public class UrlBarData {
    /** The URL schemes that don't need to be displayed complete with path. */
    public static final Set<String> SCHEMES_TO_SPLIT =
            Set.of(UrlConstants.HTTP_SCHEME, UrlConstants.HTTPS_SCHEME, UrlConstants.BLOB_SCHEME);
    /** Represents an empty URL bar. */
    public static final UrlBarData EMPTY = forNonUrlText("");

    public static UrlBarData forUrl(GURL url) {
        return forUrlAndText(url, url.isValid() ? url.getSpec() : "", null);
    }

    public static UrlBarData forNonUrlText(String displayText) {
        return create(null, displayText, 0, 0, null);
    }

    public static UrlBarData forUrlAndText(GURL url, String displayText) {
        return forUrlAndText(url, displayText, null);
    }

    /** Returns whether supplied URL should be shown in the Omnibox/Suggestions list. */
    public static boolean shouldShowUrl(GURL gurl, boolean isIncognito) {
        return !(NativePage.isNativePageUrl(gurl, isIncognito) || UrlUtilities.isNTPUrl(gurl));
    }

    public static UrlBarData forUrlAndText(
            GURL url, CharSequence displayText, @Nullable String editingText) {
        assert url.isEmpty() || url.isValid();
        int pathSearchOffset = 0;
        String displayTextStr = displayText == null ? "" : displayText.toString();
        String scheme = url.getScheme();

        if (!TextUtils.isEmpty(scheme)) {
            if (!SCHEMES_TO_SPLIT.contains(scheme)) {
                return create(url, displayTextStr, 0, displayTextStr.length(), editingText);
            }

            if (UrlConstants.BLOB_SCHEME.equals(scheme)) {
                int innerSchemeSearchOffset =
                        findFirstIndexAfterSchemeSeparator(displayText, scheme.length());
                Uri innerUri = Uri.parse(displayTextStr.substring(innerSchemeSearchOffset));
                String innerScheme = innerUri.getScheme();
                // Substitute the scheme to allow for proper display of end of inner origin.
                if (!TextUtils.isEmpty(innerScheme)) {
                    scheme = innerScheme;
                }
            }

            pathSearchOffset = findFirstIndexAfterSchemeSeparator(
                    displayText, displayTextStr.indexOf(scheme) + scheme.length());
        }
        int pathOffset = -1;
        if (displayText != null && pathSearchOffset < displayText.length()) {
            pathOffset = displayTextStr.indexOf('/', pathSearchOffset);
        }
        if (pathOffset == -1) {
            return create(url, displayText, 0, displayText == null ? 0 : displayText.length(),
                    editingText);
        }

        // If the '/' is the last character and the beginning of the path, then just drop
        // the path entirely.
        if (displayText != null && pathOffset == displayText.length() - 1) {
            return create(
                    url, displayTextStr.subSequence(0, pathOffset), 0, pathOffset, editingText);
        }

        return create(url, displayText, 0, pathOffset, editingText);
    }

    public static UrlBarData create(@Nullable GURL url, CharSequence displayText,
            int originStartIndex, int originEndIndex, @Nullable String editingText) {
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

    /**
     * The canonical URL that is shown in the URL bar, or null if it currently does not correspond
     * to a URL (for example, showing suggestion text).
     */
    public final @Nullable GURL url;

    /**
     * The text that should be shown in the URL bar. This can be a {@link Spanned} that contains
     * formatting to highlight parts of the display text.
     */
    public final CharSequence displayText;

    /**
     * The text that should replace the display text when editing the contents of the URL bar,
     * or null to use the {@link #displayText} when editing.
     */
    public final @Nullable String editingText;

    /**
     * The character index in {@link #displayText} where the origin starts. This is required to
     * ensure that the end of the origin is not scrolled out of view for long hostnames.
     */
    public final int originStartIndex;

    /**
     * The character index in {@link #displayText} where the origin ends. This is required to
     * ensure that the end of the origin is not scrolled out of view for long hostnames.
     */
    public final int originEndIndex;

    /**
     * @return The text for editing, falling back to the display text if the former is null.
     */
    public CharSequence getEditingOrDisplayText() {
        return editingText != null ? editingText : displayText;
    }

    private UrlBarData(@Nullable GURL url, CharSequence displayText, int originStartIndex,
            int originEndIndex, @Nullable String editingText) {
        this.url = url;
        this.displayText = displayText;
        this.originStartIndex = originStartIndex;
        this.originEndIndex = originEndIndex;
        this.editingText = editingText;
    }
}
