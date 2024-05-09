// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.content.Context;
import android.text.Spannable;
import android.text.style.ForegroundColorSpan;
import android.text.style.StrikethroughSpan;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.security_state.ConnectionSecurityLevel;

import java.util.Locale;

/**
 * A helper class that emphasizes the various components of a URL. Useful in the Omnibox and Page
 * Info popup where different parts of the URL should appear in different colours depending on the
 * scheme, host and connection.
 */
public class OmniboxUrlEmphasizer {
    /** Describes the components of a URL that should be emphasized. */
    public static class EmphasizeComponentsResponse {
        /** The start index of the scheme. */
        public final int schemeStart;

        /** The length of the scheme. */
        public final int schemeLength;

        /** The start index of the host. */
        public final int hostStart;

        /** The length of the host. */
        public final int hostLength;

        EmphasizeComponentsResponse(
                int schemeStart, int schemeLength, int hostStart, int hostLength) {
            this.schemeStart = schemeStart;
            this.schemeLength = schemeLength;
            this.hostStart = hostStart;
            this.hostLength = hostLength;
        }

        /** @return Whether the URL has a scheme to be emphasized. */
        public boolean hasScheme() {
            return schemeLength > 0;
        }

        /** @return Whether the URL has a host to be emphasized. */
        public boolean hasHost() {
            return hostLength > 0;
        }

        /** @return The scheme extracted from |url|, canonicalized to lowercase. */
        public String extractScheme(String url) {
            if (!hasScheme()) return "";
            return url.subSequence(schemeStart, schemeStart + schemeLength)
                    .toString()
                    .toLowerCase(Locale.US);
        }
    }

    /**
     * Parses the |text| passed in and determines the location of the scheme and host components to
     * be emphasized.
     *
     * @param autocompleteSchemeClassifier The autocomplete scheme classifier to be used for
     *     parsing.
     * @param text The text to be parsed for emphasis components.
     * @return The response object containing the locations of the emphasis components.
     */
    public static EmphasizeComponentsResponse parseForEmphasizeComponents(
            String text, AutocompleteSchemeClassifier autocompleteSchemeClassifier) {
        int[] emphasizeValues =
                OmniboxUrlEmphasizerJni.get()
                        .parseForEmphasizeComponents(text, autocompleteSchemeClassifier);
        assert emphasizeValues != null;
        assert emphasizeValues.length == 4;

        return new EmphasizeComponentsResponse(
                emphasizeValues[0], emphasizeValues[1], emphasizeValues[2], emphasizeValues[3]);
    }

    /** Denotes that a span is used for emphasizing the URL. */
    @VisibleForTesting
    public interface UrlEmphasisSpan {}

    /** Used for emphasizing the URL text by changing the text color. */
    @VisibleForTesting
    public static class UrlEmphasisColorSpan extends ForegroundColorSpan
            implements UrlEmphasisSpan {
        private int mEmphasisColor;

        /** @param color The color to set the text. */
        public UrlEmphasisColorSpan(int color) {
            super(color);
            mEmphasisColor = color;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof UrlEmphasisColorSpan)) return false;
            return ((UrlEmphasisColorSpan) obj).mEmphasisColor == mEmphasisColor;
        }

        @Override
        public String toString() {
            return getClass().getName() + ", color: " + mEmphasisColor;
        }
    }

    /** Used for emphasizing the URL text by striking through the https text. */
    @VisibleForTesting
    public static class UrlEmphasisSecurityErrorSpan extends StrikethroughSpan
            implements UrlEmphasisSpan {
        @Override
        public boolean equals(Object obj) {
            return obj instanceof UrlEmphasisSecurityErrorSpan;
        }
    }

    /**
     * Modifies the given URL to emphasize the host and scheme.
     *
     * @param url The URL spannable to add emphasis to. This variable is modified.
     * @param context Context to resolve colors against.
     * @param autocompleteSchemeClassifier The autocomplete scheme classifier used to emphasize the
     *     given URL.
     * @param securityLevel A valid ConnectionSecurityLevel for the specified web contents.
     * @param useDarkForegroundColors Whether the text colors should be dark (i.e. appropriate for
     *     use on a light background).
     * @param emphasizeScheme Whether the scheme should be emphasized.
     * @deprecated in favor of {@link #emphasizeUrl(Spannable, AutocompleteSchemeClassifier, int,
     *     boolean, int, int, int, int)} because this method doesn't support dynamic colors.
     */
    @Deprecated
    public static void emphasizeUrl(
            Spannable url,
            Context context,
            AutocompleteSchemeClassifier autocompleteSchemeClassifier,
            int securityLevel,
            boolean useDarkForegroundColors,
            boolean emphasizeScheme) {
        final @ColorRes int nonEmphasizedColorId =
                useDarkForegroundColors
                        ? R.color.url_emphasis_non_emphasized_text
                        : R.color.url_emphasis_light_non_emphasized_text;
        final @ColorRes int emphasizedColorId =
                useDarkForegroundColors
                        ? R.color.url_emphasis_emphasized_text
                        : R.color.url_emphasis_light_emphasized_text;
        final @ColorRes int dangerColorId =
                useDarkForegroundColors ? R.color.default_red_dark : R.color.default_red_light;
        final @ColorRes int secureColorId =
                useDarkForegroundColors ? R.color.default_green_dark : R.color.default_green_light;

        emphasizeUrl(
                url,
                autocompleteSchemeClassifier,
                securityLevel,
                emphasizeScheme,
                context.getColor(nonEmphasizedColorId),
                context.getColor(emphasizedColorId),
                context.getColor(dangerColorId),
                context.getColor(secureColorId));
    }

    /**
     * Modifies the given URL to emphasize the host and scheme. TODO(sashab): Make this take an
     * EmphasizeComponentsResponse object to prevent calling parseForEmphasizeComponents() again.
     *
     * @param url The URL spannable to add emphasis to. This variable is modified.
     * @param autocompleteSchemeClassifier The autocomplete scheme classifier used to emphasize the
     *     given URL.
     * @param securityLevel A valid ConnectionSecurityLevel for the specified web contents.
     * @param emphasizeScheme Whether the scheme should be emphasized.
     * @param nonEmphasizedColor Color of the non-emphasized components.
     * @param emphasizedColor Color of the emphasized components.
     * @param dangerColor Color of the scheme that denotes danger.
     * @param secureColor Color of the scheme that denotes security.
     */
    public static void emphasizeUrl(
            Spannable url,
            AutocompleteSchemeClassifier autocompleteSchemeClassifier,
            int securityLevel,
            boolean emphasizeScheme,
            @ColorInt int nonEmphasizedColor,
            @ColorInt int emphasizedColor,
            @ColorInt int dangerColor,
            @ColorInt int secureColor) {
        String urlString = url.toString();
        EmphasizeComponentsResponse emphasizeResponse =
                parseForEmphasizeComponents(urlString, autocompleteSchemeClassifier);

        int startSchemeIndex = emphasizeResponse.schemeStart;
        int endSchemeIndex = emphasizeResponse.schemeStart + emphasizeResponse.schemeLength;

        int startHostIndex = emphasizeResponse.hostStart;
        int endHostIndex = emphasizeResponse.hostStart + emphasizeResponse.hostLength;
        boolean isInternalPage =
                UrlUtilities.isInternalScheme(emphasizeResponse.extractScheme(urlString));

        // Format the scheme, if present, based on the security level.
        ForegroundColorSpan span;
        if (emphasizeResponse.hasScheme()) {
            int color = nonEmphasizedColor;
            if (!isInternalPage) {
                boolean strikeThroughScheme = false;
                switch (securityLevel) {
                    case ConnectionSecurityLevel.NONE:
                        // Intentional fall-through:
                    case ConnectionSecurityLevel.WARNING:
                        // Draw attention to the data: URI scheme for anti-spoofing reasons.
                        if (UrlConstants.DATA_SCHEME.equals(
                                emphasizeResponse.extractScheme(urlString))) {
                            color = emphasizedColor;
                        }
                        break;
                    case ConnectionSecurityLevel.DANGEROUS:
                        if (emphasizeScheme) {
                            color = dangerColor;
                        }
                        strikeThroughScheme = true;
                        break;
                    case ConnectionSecurityLevel.SECURE:
                        if (emphasizeScheme) {
                            color = secureColor;
                        }
                        break;
                    default:
                        assert false;
                }

                if (strikeThroughScheme) {
                    UrlEmphasisSecurityErrorSpan ss = new UrlEmphasisSecurityErrorSpan();
                    url.setSpan(
                            ss,
                            startSchemeIndex,
                            endSchemeIndex,
                            Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                }
            }
            span = new UrlEmphasisColorSpan(color);
            url.setSpan(span, startSchemeIndex, endSchemeIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

            // Highlight the portion of the URL visible between the scheme and the host,
            // typically :// or : depending on the scheme.
            if (emphasizeResponse.hasHost()) {
                span = new UrlEmphasisColorSpan(nonEmphasizedColor);
                url.setSpan(
                        span, endSchemeIndex, startHostIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }

        if (emphasizeResponse.hasHost()) {
            // Highlight the complete host.
            span = new UrlEmphasisColorSpan(emphasizedColor);
            url.setSpan(span, startHostIndex, endHostIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

            // Highlight the remainder of the URL.
            if (endHostIndex < urlString.length()) {
                span = new UrlEmphasisColorSpan(nonEmphasizedColor);
                url.setSpan(
                        span, endHostIndex, urlString.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        } else if (UrlConstants.DATA_SCHEME.equals(emphasizeResponse.extractScheme(urlString))) {
            // Dim the remainder of the URL for anti-spoofing purposes.
            span = new UrlEmphasisColorSpan(nonEmphasizedColor);
            url.setSpan(
                    span,
                    emphasizeResponse.schemeStart + emphasizeResponse.schemeLength,
                    urlString.length(),
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    /**
     * Reset the modifications done to emphasize components of the URL.
     *
     * @param url The URL spannable to remove emphasis from. This variable is modified.
     */
    public static void deEmphasizeUrl(Spannable url) {
        UrlEmphasisSpan[] emphasisSpans = getEmphasisSpans(url);
        if (emphasisSpans.length == 0) return;
        for (UrlEmphasisSpan span : emphasisSpans) {
            url.removeSpan(span);
        }
    }

    /**
     * Returns whether the given URL has any emphasis spans applied.
     *
     * @param url The URL spannable to check emphasis on.
     * @return True if the URL has emphasis spans, false if not.
     */
    public static boolean hasEmphasisSpans(Spannable url) {
        return getEmphasisSpans(url).length != 0;
    }

    /**
     * Returns the emphasis spans applied to the URL.
     *
     * @param url The URL spannable to get spans for.
     * @return The spans applied to the URL with emphasizeUrl().
     */
    public static UrlEmphasisSpan[] getEmphasisSpans(Spannable url) {
        return url.getSpans(0, url.length(), UrlEmphasisSpan.class);
    }

    /**
     * Returns the index of the first character containing non-origin information, or 0 if the URL
     * does not contain an origin.
     *
     * <p>For "data" URLs, the URL is not considered to contain an origin. For non-http and https
     * URLs, the whole URL is considered the origin.
     *
     * <p>For example, HTTP and HTTPS urls return the index of the first character after the domain:
     * http://www.google.com/?q=foo => 21 (up to the 'm' in google.com)
     * https://www.google.com/?q=foo => 22
     *
     * <p>Data urls always return 0, since they do not contain an origin: data:kf94hfJEj#N => 0
     *
     * <p>Other URLs treat the whole URL as an origin: file://my/pc/somewhere/foo.html => 31
     * about:blank => 11 chrome://version => 18 chrome-native://bookmarks => 25 invalidurl => 10
     *
     * <p>TODO(sashab): Make this take an EmphasizeComponentsResponse object to prevent calling
     * parseForEmphasizeComponents() again.
     *
     * @param url The URL to find the last origin character in.
     * @param autocompleteSchemeClassifier The autocomplete scheme classifier used for parsing the
     *     URL.
     * @return The index of the last character containing origin information.
     */
    public static int getOriginEndIndex(
            String url, AutocompleteSchemeClassifier autocompleteSchemeClassifier) {
        EmphasizeComponentsResponse emphasizeResponse =
                parseForEmphasizeComponents(url.toString(), autocompleteSchemeClassifier);
        if (!emphasizeResponse.hasScheme()) return url.length();

        String scheme = emphasizeResponse.extractScheme(url);

        if (scheme.equals(UrlConstants.HTTP_SCHEME) || scheme.equals(UrlConstants.HTTPS_SCHEME)) {
            return emphasizeResponse.hostStart + emphasizeResponse.hostLength;
        } else if (scheme.equals(UrlConstants.DATA_SCHEME)) {
            return 0;
        } else {
            return url.length();
        }
    }

    @NativeMethods
    public interface Natives {
        int[] parseForEmphasizeComponents(
                String text, AutocompleteSchemeClassifier autocompleteSchemeClassifier);
    }
}
