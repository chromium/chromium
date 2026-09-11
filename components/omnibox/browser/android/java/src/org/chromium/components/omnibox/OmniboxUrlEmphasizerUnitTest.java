// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableStringBuilder;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisColorSpan;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSpan;
import org.chromium.components.security_state.ConnectionSecurityLevel;

import java.util.Arrays;
import java.util.Comparator;
import java.util.Map;

/** Unit tests for {@link OmniboxUrlEmphasizer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxUrlEmphasizerUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private OmniboxUrlEmphasizerJni mOmniboxUrlEmphasizerJni;
    @Mock private AutocompleteSchemeClassifier mSchemeClassifier;

    private Context mContext;

    /** Component ranges returned by native AutocompleteInput::ParseForEmphasizeComponents. */
    private static final Map<String, int[]> URL_COMPONENTS =
            Map.ofEntries(
                    Map.entry("https://www.google.com/", new int[] {0, 5, 8, 14}),
                    Map.entry(
                            "https://www.google.com/q?query=abc123&results=1",
                            new int[] {0, 5, 8, 14}),
                    Map.entry("m.w.co/p", new int[] {-1, 0, 0, 6}),
                    Map.entry("about:blank", new int[] {0, 5, 6, 5}),
                    Map.entry(
                            "data:text/plain;charset=utf-8;base64,VGVzdCBVUkw=",
                            new int[] {0, 4, -1, 0}),
                    Map.entry("chrome://bookmarks", new int[] {0, 6, 9, 9}),
                    Map.entry("chrome-native://bookmarks", new int[] {0, 13, 16, 9}),
                    Map.entry("invalidurl", new int[] {-1, 0, 0, 10}),
                    Map.entry("", new int[] {-1, 0, -1, 0}),
                    Map.entry("http://www.google.com/", new int[] {0, 4, 7, 14}),
                    Map.entry(
                            "http://www.news.com/dir/a/b/c/page.html?foo=bar",
                            new int[] {0, 4, 7, 12}),
                    Map.entry("http://www.test.com?foo=bar", new int[] {0, 4, 7, 12}),
                    Map.entry("data:ABC123", new int[] {0, 4, -1, 0}),
                    Map.entry("data:kf94hfJEj#N", new int[] {0, 4, -1, 0}),
                    Map.entry("file://my/pc/somewhere/foo.html", new int[] {0, 4, -1, 0}),
                    Map.entry("chrome://version", new int[] {0, 6, 9, 7}));

    @Before
    public void setUp() {
        OmniboxUrlEmphasizerJni.setInstanceForTesting(mOmniboxUrlEmphasizerJni);
        mContext = ContextUtils.getApplicationContext();

        doAnswer(
                        invocation -> {
                            String text = invocation.getArgument(0);
                            int[] components = URL_COMPONENTS.get(text);
                            if (components != null) {
                                return components;
                            }
                            if (text.startsWith("data:")) {
                                return new int[] {0, 4, -1, 0};
                            }
                            throw new IllegalArgumentException("Unexpected URL in test: " + text);
                        })
                .when(mOmniboxUrlEmphasizerJni)
                .parseForEmphasizeComponents(any(), any());
    }

    private static class EmphasizedUrlSpanHelper {
        final UrlEmphasisSpan mSpan;
        final Spannable mParent;

        private EmphasizedUrlSpanHelper(UrlEmphasisSpan span, Spannable parent) {
            mSpan = span;
            mParent = parent;
        }

        private String getContents() {
            return mParent.subSequence(getStartIndex(), getEndIndex()).toString();
        }

        private int getStartIndex() {
            return mParent.getSpanStart(mSpan);
        }

        private int getEndIndex() {
            return mParent.getSpanEnd(mSpan);
        }

        private String getClassName() {
            return mSpan.getClass().getSimpleName();
        }

        private int getColorForColoredSpan() {
            return ((UrlEmphasisColorSpan) mSpan).getForegroundColor();
        }

        public static EmphasizedUrlSpanHelper[] getSpansForEmphasizedUrl(Spannable emphasizedUrl) {
            UrlEmphasisSpan[] existingSpans = OmniboxUrlEmphasizer.getEmphasisSpans(emphasizedUrl);
            EmphasizedUrlSpanHelper[] helperSpans =
                    new EmphasizedUrlSpanHelper[existingSpans.length];
            for (int i = 0; i < existingSpans.length; i++) {
                helperSpans[i] = new EmphasizedUrlSpanHelper(existingSpans[i], emphasizedUrl);
            }
            return helperSpans;
        }

        public void assertIsColoredSpan(String contents, int startIndex, int color) {
            assertEquals("Unexpected span contents:", contents, getContents());
            assertEquals(
                    "Unexpected starting index for '" + contents + "' span:",
                    startIndex,
                    getStartIndex());
            assertEquals(
                    "Unexpected ending index for '" + contents + "' span:",
                    startIndex + contents.length(),
                    getEndIndex());
            assertEquals(
                    "Unexpected class for '" + contents + "' span:",
                    UrlEmphasisColorSpan.class.getSimpleName(),
                    getClassName());
            assertEquals(
                    "Unexpected color for '" + contents + "' span:",
                    color,
                    getColorForColoredSpan());
        }

        public void assertIsStrikethroughSpan(String contents, int startIndex) {
            assertEquals("Unexpected span contents:", contents, getContents());
            assertEquals(
                    "Unexpected starting index for '" + contents + "' span:",
                    startIndex,
                    getStartIndex());
            assertEquals(
                    "Unexpected ending index for '" + contents + "' span:",
                    startIndex + contents.length(),
                    getEndIndex());
            assertEquals(
                    "Unexpected class for '" + contents + "' span:",
                    UrlEmphasisSecurityErrorSpan.class.getSimpleName(),
                    getClassName());
        }
    }

    private EmphasizedUrlSpanHelper[] getSpansForEmphasizedUrl(Spannable url) {
        EmphasizedUrlSpanHelper[] spans = EmphasizedUrlSpanHelper.getSpansForEmphasizedUrl(url);
        Arrays.sort(spans, Comparator.comparingInt(EmphasizedUrlSpanHelper::getStartIndex));
        return spans;
    }

    @Test
    public void shortSecureHttpsUrl() {
        Spannable url = new SpannableStringBuilder("https://www.google.com/");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.SECURE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 4, spans.length);
        spans[0].assertIsColoredSpan("https", 0, mContext.getColor(R.color.default_green_dark));
        spans[1].assertIsColoredSpan(
                "://", 5, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "www.google.com", 8, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[3].assertIsColoredSpan(
                "/", 22, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    @Test
    public void shortSecureHttpsUrlWithLightColors() {
        Spannable url = new SpannableStringBuilder("https://www.google.com/");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.SECURE,
                /* useDarkForegroundColors= */ false,
                /* emphasizeScheme= */ false);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 4, spans.length);
        spans[0].assertIsColoredSpan(
                "https", 0, mContext.getColor(R.color.url_emphasis_light_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                "://", 5, mContext.getColor(R.color.url_emphasis_light_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "www.google.com", 8, mContext.getColor(R.color.url_emphasis_light_emphasized_text));
        spans[3].assertIsColoredSpan(
                "/", 22, mContext.getColor(R.color.url_emphasis_light_non_emphasized_text));
    }

    @Test
    public void longInsecureHttpsUrl() {
        Spannable url =
                new SpannableStringBuilder("https://www.google.com/q?query=abc123&results=1");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.DANGEROUS,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 5, spans.length);
        spans[0].assertIsStrikethroughSpan("https", 0);
        spans[1].assertIsColoredSpan("https", 0, mContext.getColor(R.color.default_red_dark));
        spans[2].assertIsColoredSpan(
                "://", 5, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[3].assertIsColoredSpan(
                "www.google.com", 8, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[4].assertIsColoredSpan(
                "/q?query=abc123&results=1",
                22,
                mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    @Test
    public void veryShortHttpWarningUrl() {
        Spannable url = new SpannableStringBuilder("m.w.co/p");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.WARNING,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ false);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 2, spans.length);
        spans[0].assertIsColoredSpan(
                "m.w.co", 0, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[1].assertIsColoredSpan(
                "/p", 6, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    @Test
    public void aboutPageUrl() {
        Spannable url = new SpannableStringBuilder("about:blank");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 3, spans.length);
        spans[0].assertIsColoredSpan(
                "about", 0, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                ":", 5, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "blank", 6, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    @Test
    public void dataUrl() {
        Spannable url =
                new SpannableStringBuilder("data:text/plain;charset=utf-8;base64,VGVzdCBVUkw=");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 2, spans.length);
        spans[0].assertIsColoredSpan(
                "data", 0, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[1].assertIsColoredSpan(
                ":text/plain;charset=utf-8;base64,VGVzdCBVUkw=",
                4,
                mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    @Test
    public void internalChromePageUrl() {
        Spannable url = new SpannableStringBuilder("chrome://bookmarks");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 3, spans.length);
        spans[0].assertIsColoredSpan(
                "chrome", 0, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                "://", 6, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "bookmarks", 9, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    @Test
    public void internalChromeNativePageUrl() {
        Spannable url = new SpannableStringBuilder("chrome-native://bookmarks");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 3, spans.length);
        spans[0].assertIsColoredSpan(
                "chrome-native", 0, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                "://", 13, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "bookmarks", 16, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    @Test
    public void invalidUrl() {
        Spannable url = new SpannableStringBuilder("invalidurl");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 1, spans.length);
        spans[0].assertIsColoredSpan(
                "invalidurl", 0, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    @Test
    public void emptyUrl() {
        Spannable url = new SpannableStringBuilder("");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        assertEquals("Unexpected number of spans:", 0, spans.length);
    }

    @Test
    public void deEmphasizeUrl() {
        Spannable url = new SpannableStringBuilder("https://www.google.com/");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mSchemeClassifier,
                ConnectionSecurityLevel.SECURE,
                /* useDarkForegroundColors= */ true,
                /* emphasizeScheme= */ true);
        assertTrue(OmniboxUrlEmphasizer.hasEmphasisSpans(url));

        OmniboxUrlEmphasizer.deEmphasizeUrl(url);
        assertFalse(OmniboxUrlEmphasizer.hasEmphasisSpans(url));
        assertEquals(0, OmniboxUrlEmphasizer.getEmphasisSpans(url).length);
    }

    @Test
    public void httpAndHttpsUrlsOriginEndIndex() {
        String url;

        url = "http://www.google.com/";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "http://www.google.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "https://www.google.com/";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "https://www.google.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "http://www.news.com/dir/a/b/c/page.html?foo=bar";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "http://www.news.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "http://www.test.com?foo=bar";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "http://www.test.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));
    }

    @Test
    public void dataUrlsOriginEndIndex() {
        String url;

        url = "data:ABC123";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                0,
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "data:kf94hfJEj#N";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                0,
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "data:text/plain;charset=utf-8;base64,dGVzdA==";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                0,
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));
    }

    @Test
    public void otherUrlsOriginEndIndex() {
        String url;

        url = "file://my/pc/somewhere/foo.html";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "about:blank";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "chrome://version";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "chrome-native://bookmarks";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));

        url = "invalidurl";
        assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mSchemeClassifier));
    }
}
