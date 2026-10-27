// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spanned;
import android.text.SpannedString;
import android.text.style.BackgroundColorSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.ImageSpan;
import android.text.style.ScaleXSpan;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
import android.view.ContextThemeWrapper;

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
import org.chromium.chrome.browser.omnibox.R;

/** Tests for {@link SuggestionSpannable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionSpannableUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Context mContext;
    @Mock private ColorStateList mColor1;
    @Mock private ColorStateList mColor2;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void spannableEquals_basicStringsComparisonOfSamePlainStrings() {
        SuggestionSpannable span = new SuggestionSpannable("test string");
        assertTrue(span.equals(new SuggestionSpannable("test string")));
    }

    @Test
    public void spannableEquals_basicStringsComparisonOfDifferentPlainStrings() {
        final SuggestionSpannable span = new SuggestionSpannable("test string");
        assertFalse(span.equals(new SuggestionSpannable("different test string")));
        assertFalse(span.equals(new SuggestionSpannable("")));
        assertFalse(span.equals("test string"));
        assertFalse(span.equals(null));
    }

    @Test
    public void spannableEquals_basicStringsComparisonOfSameRichStrings() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertTrue(c1.equals(c2));
    }

    @Test
    public void spannableEquals_differentSpanType() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_differentTypeface() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.ITALIC), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_differentSpanRangeStart() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 5, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_differentSpanRangeEnd() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 4, 6, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_missingAttributesInSource() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 7, 8, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_missingAttributesInOther() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c1.setSpan(new StyleSpan(Typeface.BOLD), 7, 8, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 4, 7, Spanned.SPAN_EXCLUSIVE_INCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_testReflexivity() {
        // Some android versions are susceptible to reordering bug:
        // internal getSpan() method can rearrange returned items, while direct access to
        // mSpans structure lists them in order of appearance.
        // See http://b2/73359036
        SuggestionSpannable c1 = new SuggestionSpannable("test string");

        Object span1 = new Object();
        c1.setSpan(span1, 1, 5, 0);

        Object span2 = new Object();
        c1.setSpan(span2, 0, 5, Spanned.SPAN_PRIORITY);

        SuggestionSpannable c2 = c1;

        assertTrue(c1.equals(c2));
        assertTrue(c2.equals(c1));
    }

    @Test
    public void spannableEquals_worksWithSpansNotOverridingEquals() {
        // Some span styles don't directly override .equals.
        // We don't really care much about the details of these individual spans as long as
        // we only use a limited subset of them that we know well. We care however that our
        // equals call does not give us false negatives.
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        ForegroundColorSpan s1 = new ForegroundColorSpan(0);
        ForegroundColorSpan s2 = new ForegroundColorSpan(0);

        c1.setSpan(s1, 0, 5, 0);
        c2.setSpan(s2, 0, 5, 0);

        assertTrue(c1.equals(c2));
        assertTrue(c2.equals(c1));
    }

    @Test
    public void spannableEquals_differentSpansAtSame() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new BackgroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new ForegroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_backgroundColorSpansSame() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new BackgroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new BackgroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertTrue(c1.equals(c2));
    }

    @Test
    public void spannableEquals_backgroundColorSpansDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new BackgroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new BackgroundColorSpan(1), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_foregroundColorSpansSame() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new ForegroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new ForegroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertTrue(c1.equals(c2));
    }

    @Test
    public void spannableEquals_foregroundColorSpansDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new ForegroundColorSpan(0), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new ForegroundColorSpan(1), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_styleSpansSame() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertTrue(c1.equals(c2));
    }

    @Test
    public void spannableEquals_styleSpansDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new StyleSpan(Typeface.BOLD), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new StyleSpan(Typeface.BOLD_ITALIC), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_fontFamilyDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan("A", 0, 0, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan("B", 0, 0, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_fontStyleDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan("A", 1, 0, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan("A", 2, 0, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_fontSizeDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan("A", 0, 1, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan("A", 0, 2, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_fontColorDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan("A", 0, 0, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan("A", 0, 0, mColor2, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_fontLinkDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan("A", 0, 0, mColor1, mColor1),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan("A", 0, 0, mColor1, mColor2),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_textAppearanceSpansSame() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextLarge_Primary),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextLarge_Primary),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertTrue(c1.equals(c2));
    }

    @Test
    public void spannableEquals_textAppearanceSpansDifferent() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextSmall_Disabled),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(
                new TextAppearanceSpan(mContext, R.style.TextAppearance_TextMedium_Secondary),
                3,
                5,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_emptyStringsAreConsideredSame() {
        SuggestionSpannable c1 = new SuggestionSpannable("");
        SuggestionSpannable c2 = new SuggestionSpannable("");

        c1.setSpan(new ScaleXSpan(1.0f), 0, 0, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new ScaleXSpan(1.1f), 0, 0, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        assertTrue(c1.equals(c2));
    }

    @Test
    public void spannableEquals_unknownSpanTypesAreNeverEqual() {
        SuggestionSpannable c1 = new SuggestionSpannable("test string");
        SuggestionSpannable c2 = new SuggestionSpannable("test string");

        c1.setSpan(new ScaleXSpan(1.0f), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        c2.setSpan(new ScaleXSpan(1.0f), 3, 5, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        // Not understood by equals(): conservatively reported as different, without asserting.
        assertFalse(c1.equals(c2));
    }

    @Test
    public void spannableEquals_multiSpan_identicalExtents() {
        assertTrue(
                twoBoldRegionsPlusColor(0, 9, 10, 17)
                        .equals(twoBoldRegionsPlusColor(0, 9, 10, 17)));
    }

    @Test
    public void spannableEquals_multiSpan_sameCountAndTypes_firstExtentGrows() {
        // Typing one more character extends the first highlight by one.
        assertFalse(
                twoBoldRegionsPlusColor(0, 9, 10, 17)
                        .equals(twoBoldRegionsPlusColor(0, 10, 10, 17)));
    }

    @Test
    public void spannableEquals_multiSpan_sameCountAndTypes_secondExtentMoves() {
        assertFalse(
                twoBoldRegionsPlusColor(0, 9, 10, 17)
                        .equals(twoBoldRegionsPlusColor(0, 9, 11, 17)));
    }

    @Test
    public void spannableEquals_multiSpan_sameExtentsAssignedInDifferentOrder() {
        // The same multiset of extents attached to the two bold spans in swapped insertion order.
        // The comparison is positional, so this is (conservatively) not equal. This pins that
        // differing extents can never be masked by span count/type matching.
        assertFalse(
                twoBoldRegionsPlusColor(0, 9, 10, 17)
                        .equals(twoBoldRegionsPlusColor(10, 17, 0, 9)));
    }

    @Test
    public void spannableEquals_multiSpan_spanCountDiffers() {
        SuggestionSpannable oneBold = new SuggestionSpannable("developer.android.com/reference");
        oneBold.setSpan(new StyleSpan(Typeface.BOLD), 0, 9, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        oneBold.setSpan(
                new ForegroundColorSpan(Color.RED),
                0,
                oneBold.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        assertFalse(twoBoldRegionsPlusColor(0, 9, 10, 17).equals(oneBold));
        assertFalse(oneBold.equals(twoBoldRegionsPlusColor(0, 9, 10, 17)));
    }

    @Test
    public void spannableEquals_imageSpan_sameDrawableInstance() {
        Drawable icon = new ColorDrawable(Color.RED);
        assertTrue(
                withImageSpan(icon, ImageSpan.ALIGN_CENTER)
                        .equals(withImageSpan(icon, ImageSpan.ALIGN_CENTER)));
    }

    @Test
    public void spannableEquals_imageSpan_differentAlignment() {
        Drawable icon = new ColorDrawable(Color.RED);
        assertFalse(
                withImageSpan(icon, ImageSpan.ALIGN_CENTER)
                        .equals(withImageSpan(icon, ImageSpan.ALIGN_BASELINE)));
    }

    @Test
    public void spannableEquals_imageSpan_distinctDrawablesAreNeverEqual() {
        // Drawables have no meaningful equals(), so even a look-alike instance is reported as
        // different (a false negative, never a false positive).
        assertFalse(
                withImageSpan(new ColorDrawable(Color.RED), ImageSpan.ALIGN_CENTER)
                        .equals(
                                withImageSpan(
                                        new ColorDrawable(Color.RED), ImageSpan.ALIGN_CENTER)));
    }

    @Test
    public void contentEquals_matchesTextViewStyleCopy() {
        // TextView.setText keeps a SpannedString copy of what it was given; contentEquals must
        // treat that copy as equal to the original so no-op rebinds can be detected against it.
        SuggestionSpannable original = twoBoldRegionsPlusColor(0, 9, 10, 17);
        assertTrue(SuggestionSpannable.contentEquals(new SpannedString(original), original));
        assertTrue(SuggestionSpannable.contentEquals(original, new SpannedString(original)));
        assertFalse(
                SuggestionSpannable.contentEquals(
                        new SpannedString(original), twoBoldRegionsPlusColor(0, 10, 10, 17)));
    }

    /**
     * Build the shape of text BasicSuggestionProcessor produces: two bold match regions plus a
     * full-length foreground colour span, in that insertion order.
     */
    private static SuggestionSpannable twoBoldRegionsPlusColor(
            int bold1Start, int bold1End, int bold2Start, int bold2End) {
        SuggestionSpannable s = new SuggestionSpannable("developer.android.com/reference");
        s.setSpan(
                new StyleSpan(Typeface.BOLD),
                bold1Start,
                bold1End,
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        s.setSpan(
                new StyleSpan(Typeface.BOLD),
                bold2Start,
                bold2End,
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        s.setSpan(
                new ForegroundColorSpan(Color.RED),
                0,
                s.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return s;
    }

    private static SuggestionSpannable withImageSpan(Drawable icon, int alignment) {
        SuggestionSpannable s = new SuggestionSpannable(" x");
        s.setSpan(new ImageSpan(icon, alignment), 0, 1, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        return s;
    }
}
