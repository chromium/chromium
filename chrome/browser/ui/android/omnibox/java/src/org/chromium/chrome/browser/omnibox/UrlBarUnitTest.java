// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.graphics.Paint;
import android.text.InputType;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewStructure;
import android.view.inputmethod.EditorInfo;

import androidx.core.view.inputmethod.EditorInfoCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowPaint;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.Features.JUnitProcessor;

import java.util.Collections;

/**
 * Unit tests for the URL bar UI component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w100dp-h50dp")
public class UrlBarUnitTest {
    private UrlBar mUrlBar;
    public @Rule TestRule mFeaturesProcessorRule = new JUnitProcessor();
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock UrlBarDelegate mUrlBarDelegate;
    private @Mock ViewStructure mViewStructure;

    private final String mShortPath = "/aaaa";
    private final String mLongPath =
            "/" + TextUtils.join("", Collections.nCopies(UrlBar.MIN_LENGTH_FOR_TRUNCATION, "a"));
    private final String mShortDomain = "www.a.com";
    private final String mLongDomain = "www."
            + TextUtils.join("", Collections.nCopies(UrlBar.MIN_LENGTH_FOR_TRUNCATION, "a"))
            + ".com";

    // Screen width is set to 100px, with a default density of 1px per dp, and we estimate 5dp per
    // char, so there will be 20 visible characters.
    private final int mNumberOfVisibleCharacters = 20;

    @Implements(Layout.class)
    public static class ShadowLayout {
        @Implementation
        public float getPrimaryHorizontal(int offset) {
            return (float) offset * 5;
        }

        // TODO(peilinwang) remove once ScrollToTLDOptimization experiment is finished. This is
        // only needed for bots that run testss with disable_fieldtrial_testing_config=true.
        @Implementation
        public int getOffsetForHorizontal(int line, float horiz) {
            return (int) horiz / 5;
        }
    }

    @Implements(Paint.class)
    public static class MyShadowPaint extends ShadowPaint {
        @Implementation
        public int getOffsetForAdvance(CharSequence text, int start, int end, int contextStart,
                int contextEnd, boolean isRtl, float advance) {
            return (int) advance / 5;
        }
    }

    @Before
    public void setUp() {
        mUrlBar = spy(new UrlBarApi26(ContextUtils.getApplicationContext(), null));
        mUrlBar.setDelegate(mUrlBarDelegate);

        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mUrlBar.setLayoutParams(params);
    }

    @Test
    public void testAutofillStructureReceivesFullURL() {
        mUrlBar.setTextForAutofillServices("https://www.google.com");
        mUrlBar.setText("www.google.com");
        mUrlBar.onProvideAutofillStructure(mViewStructure, 0);

        ArgumentCaptor<SpannableStringBuilder> haveUrl =
                ArgumentCaptor.forClass(SpannableStringBuilder.class);
        verify(mViewStructure).setText(haveUrl.capture());
        assertEquals("https://www.google.com", haveUrl.getValue().toString());
    }

    @Test
    public void onCreateInputConnection_ensureNoAutocorrect() {
        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);
        assertEquals(EditorInfo.TYPE_TEXT_VARIATION_URI,
                info.inputType & EditorInfo.TYPE_TEXT_VARIATION_URI);
        assertEquals(0, info.inputType & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);
    }

    @Test
    public void onCreateInputConnection_disallowKeyboardLearningPassedToIme() {
        doReturn(true).when(mUrlBarDelegate).allowKeyboardLearning();

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);
        assertEquals(0, info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
    }

    @Test
    public void onCreateInputConnection_allowKeyboardLearningPassedToIme() {
        doReturn(false).when(mUrlBarDelegate).allowKeyboardLearning();

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);
        assertEquals(EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
                info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
    }

    @Test
    public void onCreateInputConnection_setDefaultsWhenDelegateNotPresent() {
        mUrlBar.setDelegate(null);

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);

        assertEquals(EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
                info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
        assertEquals(EditorInfo.TYPE_TEXT_VARIATION_URI,
                info.inputType & EditorInfo.TYPE_TEXT_VARIATION_URI);
        assertEquals(0, info.inputType & EditorInfo.TYPE_TEXT_FLAG_AUTO_CORRECT);
    }

    @Test
    public void urlBar_editorSetupPermitsWordSelection() {
        // This test verifies whether the Omnibox is set up so that it permits word selection. See:
        // https://cs.android.com/search?q=function:Editor.needsToSelectAllToSelectWordOrParagraph

        int klass = mUrlBar.getInputType() & InputType.TYPE_MASK_CLASS;
        int variation = mUrlBar.getInputType() & InputType.TYPE_MASK_VARIATION;
        int flags = mUrlBar.getInputType() & InputType.TYPE_MASK_FLAGS;
        assertEquals(InputType.TYPE_CLASS_TEXT, klass);
        assertEquals(InputType.TYPE_TEXT_VARIATION_NORMAL, variation);
        assertEquals(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS, flags);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testTruncation_LongUrl() {
        String url = mShortDomain + mLongPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        String text = mUrlBar.getText().toString();
        assertEquals(url.substring(0, mNumberOfVisibleCharacters), text);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testTruncation_ShortUrl() {
        String url = mShortDomain + mShortPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        String text = mUrlBar.getText().toString();
        assertEquals(url, text);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testTruncation_LongTld_ScrollToTld() {
        String url = mLongDomain + mShortPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mLongDomain.length());
        String text = mUrlBar.getText().toString();
        assertEquals(mLongDomain, text);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testTruncation_LongTld_ScrollToBeginning() {
        String url = mShortDomain + mLongPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        String text = mUrlBar.getText().toString();
        assertEquals(url.substring(0, mNumberOfVisibleCharacters), text);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testTruncation_NoTruncationForWrapContent() {
        LayoutParams previousLayoutParams = mUrlBar.getLayoutParams();
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT);
        mUrlBar.setLayoutParams(params);

        mUrlBar.setTextWithTruncation(mLongDomain, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        String text = mUrlBar.getText().toString();
        assertEquals(mLongDomain, text);

        mUrlBar.setLayoutParams(previousLayoutParams);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testTruncation_NoTruncationWhileFocused() {
        mUrlBar.onFocusChanged(true, 0, null);

        mUrlBar.setTextWithTruncation(mLongDomain, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        String text = mUrlBar.getText().toString();
        assertEquals(mLongDomain, text);

        mUrlBar.onFocusChanged(false, 0, null);
    }

    @Test
    public void testOnTouchEvent_handleTouchAfterFocus() {
        mUrlBar.onFocusChanged(true, View.FOCUS_DOWN, null);
        mUrlBar.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0));
        verify(mUrlBarDelegate).onTouchAfterFocus();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NO_VISIBLE_HINT_FOR_TABLETS)
    @Config(qualifiers = "w600dp-h820dp", shadows = {ShadowLayout.class, MyShadowPaint.class})
    public void testNoVisibleHintCalculationForTablets_noHistogramRecords() {
        // Ensure that Views and Layouts aren't null.
        mUrlBar.measure(MeasureSpec.makeMeasureSpec(820, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(20, MeasureSpec.EXACTLY));
        mUrlBar.layout(0, 0, mUrlBar.getMeasuredWidth(), mUrlBar.getMeasuredHeight());

        // Avoid short circuiting in scrollDisplayText().
        doReturn(false).when(mUrlBar).isLayoutRequested();

        String url = mShortDomain + mLongPath;
        mUrlBar.setText(url);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.CalculateVisibleHint.Duration")
                        .expectNoRecords("Omnibox.NumberOfVisibleCharacters")
                        .build();
        mUrlBar.setScrollState(UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        histogramWatcher.assertExpected();
    }

    @Test
    @Config(shadows = {ShadowLayout.class, MyShadowPaint.class})
    public void testVisibleHintCalculationHistograms() {
        // Ensure that Views and Layouts aren't null.
        mUrlBar.measure(MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(10, MeasureSpec.EXACTLY));
        mUrlBar.layout(0, 0, mUrlBar.getMeasuredWidth(), mUrlBar.getMeasuredHeight());

        // Avoid short circuiting in scrollDisplayText().
        doReturn(false).when(mUrlBar).isLayoutRequested();

        String url = mShortDomain + mLongPath;
        mUrlBar.setText(url);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Omnibox.CalculateVisibleHint.Duration")
                        .expectIntRecord(
                                "Omnibox.NumberOfVisibleCharacters", mNumberOfVisibleCharacters)
                        .build();
        mUrlBar.setScrollState(UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        histogramWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testSetLengtHistogram_noTruncation() {
        String url = mShortDomain + mLongPath;

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Omnibox.SetText.TextLength", url.length());
        mUrlBar.setText(url);
        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VISIBLE_URL_TRUNCATION)
    public void testSetLengtHistogram_withTruncation() {
        String url = mShortDomain + mLongPath;

        HistogramWatcher histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Omnibox.SetText.TextLength", mNumberOfVisibleCharacters);
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        histogramWatcher.assertExpected();
    }
}
