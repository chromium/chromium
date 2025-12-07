// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.graphics.Paint;
import android.graphics.Rect;
import android.text.InputType;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewStructure;
import android.view.inputmethod.EditorInfo;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.core.view.inputmethod.EditorInfoCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.TestActivity;

import java.util.Collections;
import java.util.List;

/** Unit tests for the URL bar UI component. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w100dp-h50dp")
public class UrlBarUnitTest {
    // UrlBar has 4 px of padding on the left and right. Set this to urlbar width + padding so
    // getVisibleMeasuredViewportWidth() returns 100. This ensures NUMBER_OF_VISIBLE_CHARACTERS
    // is accurate.
    private static final int URL_BAR_WIDTH = 100 + 8;
    private static final int URL_BAR_HEIGHT = 50;
    private static final float FONT_HEIGHT_NOMINAL = 100f;
    private static final float FONT_HEIGHT_ACTUAL_TALL = 120f;
    private static final float FONT_HEIGHT_ACTUAL_SHORT = 80f;
    private static final float LINE_HEIGHT_REGULAR_FACTOR = UrlBar.LINE_HEIGHT_FACTOR;
    private static final float LINE_HEIGHT_ELEGANT_FACTOR = 1.6f;

    // Screen width is set to 100px, with a default density of 1px per dp, and we estimate 5dp per
    // char, so there will be 20 visible characters.
    private static final int NUMBER_OF_VISIBLE_CHARACTERS = 20;

    // Separately declare a constant same as UrlBar.MIN_LENGTH_FOR_TRUNCATION so that one of these
    // tests will fail if it's accidentally changed.
    private static final int MIN_LENGTH_FOR_TRUNCATION = 100;

    private ActivityController<TestActivity> mController;
    private Activity mActivity;
    private UrlBar mUrlBar;
    private final Paint.FontMetrics mFontMetrics = new Paint.FontMetrics();
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock UrlBarDelegate mUrlBarDelegate;
    private @Mock ViewStructure mViewStructure;
    private @Mock Layout mLayout;
    private @Mock TextPaint mPaint;

    private int mLastTextDirection;
    private int mLastTextAlignment;

    private final String mShortPath = "/aaaa";
    private final String mLongPath =
            "/" + TextUtils.join("", Collections.nCopies(MIN_LENGTH_FOR_TRUNCATION, "a"));
    private final String mShortDomain = "www.a.com";
    private final String mLongDomain =
            "www."
                    + TextUtils.join("", Collections.nCopies(MIN_LENGTH_FOR_TRUNCATION, "a"))
                    + ".com";

    @Before
    public void setUp() {
        mController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        var layout = new FrameLayout(mActivity);
        mActivity.setContentView(layout);

        UrlBar urlBar =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.url_bar, layout, true)
                        .findViewById(R.id.url_bar);

        mUrlBar = spy(urlBar);
        mUrlBar.setDelegate(mUrlBarDelegate);

        mLastTextDirection = -1;
        mLastTextAlignment = -1;

        doAnswer(i -> mLastTextDirection = i.getArgument(0))
                .when(mUrlBar)
                .setTextDirection(anyInt());
        doAnswer(i -> mLastTextAlignment = i.getArgument(0))
                .when(mUrlBar)
                .setTextAlignment(anyInt());

        lenient().doReturn(1).when(mLayout).getLineCount();
        lenient()
                .doAnswer(invocation -> (int) invocation.getArguments()[0] * 5f)
                .when(mLayout)
                .getPrimaryHorizontal(anyInt());

        lenient()
                .doAnswer(invocation -> (int) ((float) invocation.getArguments()[6] / 5))
                .when(mPaint)
                .getOffsetForAdvance(
                        any(CharSequence.class),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        anyFloat());

        lenient().doReturn(mFontMetrics).when(mPaint).getFontMetrics();
        lenient().doReturn(mPaint).when(mUrlBar).getPaint();
    }

    @After
    public void tearDown() {
        mController.close();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    /** Force reset text layout. */
    private void resetTextLayout() {
        mUrlBar.nullLayouts();
        assertNull(mUrlBar.getLayout());
    }

    /**
     * Simulate measure() and layout() pass on the view. Ensures view size and text layout are
     * resolved.
     */
    private void measureAndLayoutUrlBarForSize(int width, int height) {
        // Measure and layout the Url bar.
        mUrlBar.setLayoutParams(new LayoutParams(width, height));
        mUrlBar.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        mUrlBar.layout(0, 0, width, height);

        // Confirmation check: new layout should be available.
        assertNotNull(mUrlBar.getLayout());
        assertFalse(mUrlBar.isLayoutRequested());
    }

    /** Resize the UrlBar to its default size for testing. */
    private void measureAndLayoutUrlBar() {
        measureAndLayoutUrlBarForSize(URL_BAR_WIDTH, URL_BAR_HEIGHT);
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
        assertEquals(
                EditorInfo.TYPE_TEXT_VARIATION_URI,
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
        assertEquals(
                EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
                info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
    }

    @Test
    public void onCreateInputConnection_setDefaultsWhenDelegateNotPresent() {
        mUrlBar.setDelegate(null);

        var info = new EditorInfo();
        mUrlBar.onCreateInputConnection(info);

        assertEquals(
                EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING,
                info.imeOptions & EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING);
        assertEquals(
                EditorInfo.TYPE_TEXT_VARIATION_URI,
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
    public void testTruncation_LongUrl() {
        doReturn(mLayout).when(mUrlBar).getLayout();
        measureAndLayoutUrlBar();
        String url = mShortDomain + mLongPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        String text = mUrlBar.getText().toString();
        assertEquals(url.substring(0, NUMBER_OF_VISIBLE_CHARACTERS), text);
    }

    @Test
    public void testTruncation_ShortUrl() {
        // Test with a url one character shorter than the minimum length for truncation so that this
        // test fails when the UrlBar.MIN_LENGTH_FOR_TRUCATION_V2 is changed to something smaller.
        String url = mShortDomain + mLongPath;
        url = url.substring(0, 99);
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        String text = mUrlBar.getText().toString();
        assertEquals(url, text);
    }

    @Test
    public void testTruncation_LongTld_ScrollToTld() {
        doReturn(mLayout).when(mUrlBar).getLayout();
        measureAndLayoutUrlBar();
        String url = mLongDomain + mShortPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_TLD, mLongDomain.length());
        String text = mUrlBar.getText().toString();
        assertEquals(mLongDomain, text);
    }

    @Test
    public void testTruncation_LongTld_ScrollToBeginning() {
        doReturn(mLayout).when(mUrlBar).getLayout();
        measureAndLayoutUrlBar();
        String url = mShortDomain + mLongPath;
        mUrlBar.setTextWithTruncation(url, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        String text = mUrlBar.getText().toString();
        assertEquals(url.substring(0, NUMBER_OF_VISIBLE_CHARACTERS), text);
    }

    @Test
    public void testTruncation_NoTruncationForWrapContent() {
        measureAndLayoutUrlBar();
        LayoutParams previousLayoutParams = (LayoutParams) mUrlBar.getLayoutParams();
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT);
        mUrlBar.setLayoutParams(params);

        mUrlBar.setTextWithTruncation(mLongDomain, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        String text = mUrlBar.getText().toString();
        assertEquals(mLongDomain, text);

        mUrlBar.setLayoutParams(previousLayoutParams);
    }

    @Test
    public void performClick_emitsTouchEvents() {
        mUrlBar.performClick();
        verify(mUrlBarDelegate).onFocusByTouch();
        // No subsequent events.
        mUrlBar.performClick();
        verifyNoMoreInteractions(mUrlBarDelegate);

        // Simulate focus lost, then applied programmatically.
        // This will reset the internal state, and then enable alternative event.
        mUrlBar.onFocusChanged(false, 0, null);
        mUrlBar.onFocusChanged(true, 0, null);

        mUrlBar.performClick();
        verify(mUrlBarDelegate).onTouchAfterFocus();
        // No subsequent events.
        mUrlBar.performClick();
        verifyNoMoreInteractions(mUrlBarDelegate);
    }

    @Test
    public void onTouchEvent_touchDownIsIgnored() {
        mUrlBar.onFocusChanged(true, View.FOCUS_DOWN, null);
        mUrlBar.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0));
        verify(mUrlBarDelegate, never()).onTouchAfterFocus();
    }

    @Test
    public void onTouchEvent_touchUpEmitsTouchEvents() {
        mUrlBar.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0));
        verify(mUrlBarDelegate).onFocusByTouch();
        // No subsequent events.
        mUrlBar.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0));
        verifyNoMoreInteractions(mUrlBarDelegate);

        // Simulate focus lost, then applied programmatically.
        // This will reset the internal state, and then enable alternative event.
        mUrlBar.onFocusChanged(false, 0, null);
        mUrlBar.onFocusChanged(true, 0, null);

        mUrlBar.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0));
        verify(mUrlBarDelegate).onTouchAfterFocus();
        // No subsequent events.
        mUrlBar.onTouchEvent(MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0));
        verifyNoMoreInteractions(mUrlBarDelegate);
    }

    /**
     * Mouse and touchpad requires special handling for clicks. If we don't gain focus, we
     * explicitly fire focus for these input types.
     */
    @Test
    public void onTouchEvent_ensureTouchpadFocusFired() {
        // 1. Fire a touchpad event
        MotionEvent evt = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        evt.setSource(InputDevice.SOURCE_TOUCHPAD);
        mUrlBar.onTouchEvent(evt);

        // 2. Fire a mouse event
        evt.setSource(InputDevice.SOURCE_MOUSE);
        mUrlBar.onTouchEvent(evt);

        // // 3. Fire a touchscreen event
        evt.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        mUrlBar.onTouchEvent(evt);

        // Expect only two explicit calls to request focus (mouse and touchpad only)
        verify(mUrlBar, times(2)).requestFocus();
    }

    @Test
    public void onTouchEvent_ensureTouchpadFocusFiredSkipped() {
        doReturn(true).when(mUrlBar).isFocused();

        // 1. Fire a touchpad event
        MotionEvent evt = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        evt.setSource(InputDevice.SOURCE_TOUCHPAD);
        mUrlBar.onTouchEvent(evt);

        // 2. Fire a mouse event
        evt.setSource(InputDevice.SOURCE_MOUSE);
        mUrlBar.onTouchEvent(evt);

        // // 3. Fire a touchscreen event
        evt.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        mUrlBar.onTouchEvent(evt);

        // Since we already have focus, expect zero explicit requests to gain focus.
        verify(mUrlBar, times(0)).requestFocus();
    }

    @Test
    public void performClick_emittedOnlyOnce() {
        mUrlBar.performClick();
        verify(mUrlBarDelegate).onFocusByTouch();

        clearInvocations(mUrlBarDelegate);

        mUrlBar.performClick();
        verifyNoMoreInteractions(mUrlBarDelegate);

        // Simluate focus lost. This should re-set recorded state and permit the UrlBar to emit
        // focus events once more.
        mUrlBar.onFocusChanged(false, 0, null);

        mUrlBar.performClick();
        verify(mUrlBarDelegate).onFocusByTouch();
    }

    @Test
    public void performClick_safeWithNoDelegate() {
        mUrlBar.setDelegate(null);
        mUrlBar.performClick();
    }

    @Test
    public void testTruncation_NoTruncationWhileFocused() {
        mUrlBar.onFocusChanged(true, 0, null);

        mUrlBar.setTextWithTruncation(mLongDomain, UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        String text = mUrlBar.getText().toString();
        assertEquals(mLongDomain, text);

        mUrlBar.onFocusChanged(false, 0, null);
    }

    @Test
    public void
            scrollToBeginning_fallBackToDefaultWhenLayoutUnavailable_ltrLayout_noText_ltrHint() {
        // Explicitly invalidate text layouts. This could happen for a number of reasons.
        // This is also the implicit default value until text is measured, but don't rely on this.
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mUrlBar).getLayoutDirection();
        mUrlBar.setHint("hint text");
        resetTextLayout();

        // As long as layouts are not available, no action should be taken.
        // This is typically the case when the text view or content is manipulated in some way and
        // has not yet completed the full measure/layout cycle.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollTo(anyInt(), anyInt());
        assertTrue(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // LTR layout always scrolls to 0, because that's the natural origin of LTR text.
        measureAndLayoutUrlBar();
        verify(mUrlBar).scrollTo(0, 0);
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate request to update scroll type with no changes of scroll type, text, or view
        // size. This should avoid recalculations and simply re-set the scroll position.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollToTLD();
        verify(mUrlBar, never()).scrollToBeginning();
        verify(mUrlBar).scrollTo(0, 0);
    }

    @Test
    public void
            scrollToBeginning_fallBackToDefaultWhenLayoutUnavailable_rtlLayout_noText_ltrHint() {
        // Explicitly invalidate text layouts. This could happen for a number of reasons.
        // This is also the implicit default value until text is measured, but don't rely on this.
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mUrlBar).getLayoutDirection();
        mUrlBar.setHint("hint text");
        resetTextLayout();

        // As long as layouts are not available, no action should be taken.
        // This is typically the case when the text view or content is manipulated in some way and
        // has not yet completed the full measure/layout cycle.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollTo(anyInt(), anyInt());
        assertTrue(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // RTL layouts should scroll to 0 too, because that's the natural origin of LTR text.
        measureAndLayoutUrlBar();
        verify(mUrlBar).scrollTo(0, 0);
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate request to update scroll type with no changes of scroll type, text, or view
        // size. This should avoid recalculations and simply re-set the scroll position.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollToTLD();
        verify(mUrlBar, never()).scrollToBeginning();
        verify(mUrlBar).scrollTo(0, 0);
    }

    @Test
    public void
            scrollToBeginning_fallBackToDefaultWhenLayoutUnavailable_ltrLayout_noText_rtlHint() {
        // Explicitly invalidate text layouts. This could happen for a number of reasons.
        // This is also the implicit default value until text is measured, but don't rely on this.
        doReturn(View.LAYOUT_DIRECTION_LTR).when(mUrlBar).getLayoutDirection();
        mUrlBar.setHint("טקסט רמז");
        resetTextLayout();

        // As long as layouts are not available, no action should be taken.
        // This is typically the case when the text view or content is manipulated in some way and
        // has not yet completed the full measure/layout cycle.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollTo(anyInt(), anyInt());
        assertTrue(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // LTR layout always scrolls to 0, even if the hint text is RTL. View hierarchy dictates the
        // layout direction.
        measureAndLayoutUrlBar();
        verify(mUrlBar).scrollTo(0, 0);
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate request to update scroll type with no changes of scroll type, text, or view
        // size. This should avoid recalculations and simply re-set the scroll position.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollToTLD();
        verify(mUrlBar, never()).scrollToBeginning();
        verify(mUrlBar).scrollTo(0, 0);
    }

    @Test
    public void
            scrollToBeginning_fallBackToDefaultWhenLayoutUnavailable_rtlLayout_noText_rtlHint() {
        // Explicitly invalidate text layouts. This could happen for a number of reasons.
        // This is also the implicit default value until text is measured, but don't rely on this.
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mUrlBar).getLayoutDirection();
        mUrlBar.setHint("טקסט רמז");
        resetTextLayout();

        // As long as layouts are not available, no action should be taken.
        // This is typically the case when the text view or content is manipulated in some way and
        // has not yet completed the full measure/layout cycle.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollTo(anyInt(), anyInt());
        assertTrue(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // RTL layout should position RTL text at an appropriate offset relative to view end.
        measureAndLayoutUrlBar();
        verify(mUrlBar).scrollTo(not(eq(0)), eq(0));
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate request to update scroll type with no changes of scroll type, text, or view
        // size. This should avoid recalculations and simply re-set the scroll position.
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        verify(mUrlBar, never()).scrollToTLD();
        verify(mUrlBar, never()).scrollToBeginning();
        verify(mUrlBar).scrollTo(not(eq(0)), eq(0));
    }

    @Test
    public void layout_noScrollWithNoSizeChanges() {
        // Initialize the URL bar. Verify test conditions.
        mUrlBar.setText(mShortDomain);
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        measureAndLayoutUrlBar();
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate layout re-entry.
        // We know the url bar has no pending scroll request, and we apply the same size.
        measureAndLayoutUrlBar();
        verify(mUrlBar, never()).scrollDisplayText(anyInt());
    }

    @Test
    public void layout_noScrollWhenHeightChanges() {
        // Initialize the URL bar. Verify test conditions.
        mUrlBar.setText(mShortDomain);
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        measureAndLayoutUrlBar();
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate layout re-entry.
        // We change the height of the view which should not affect scroll position.
        measureAndLayoutUrlBarForSize(URL_BAR_WIDTH, URL_BAR_HEIGHT + 1);
        verify(mUrlBar, never()).scrollDisplayText(anyInt());
    }

    @Test
    public void layout_updateScrollWhenWidthChanges() {
        // Initialize the URL bar. Verify test conditions.
        mUrlBar.setText(mShortDomain);
        mUrlBar.scrollDisplayText(UrlBar.ScrollType.SCROLL_TO_BEGINNING);
        measureAndLayoutUrlBar();
        assertFalse(mUrlBar.hasPendingDisplayTextScrollForTesting());
        clearInvocations(mUrlBar);

        // Simulate layout re-entry.
        // We change the width, which may impact scroll position.
        measureAndLayoutUrlBarForSize(URL_BAR_WIDTH + 1, URL_BAR_HEIGHT);
        verify(mUrlBar).scrollDisplayText(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD)
    public void scrollToTLD_sameTLD_calculateVisibleHint() {
        doReturn(mLayout).when(mUrlBar).getLayout();
        doReturn(mPaint).when(mLayout).getPaint();

        measureAndLayoutUrlBar();
        // Url needs to be long enough to fill the entire url bar.
        String url =
                mShortDomain
                        + "/"
                        + TextUtils.join(
                                "", Collections.nCopies(NUMBER_OF_VISIBLE_CHARACTERS, "a"));
        mUrlBar.setText(url);
        mUrlBar.setScrollState(UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        verify(mUrlBar, times(0)).calculateVisibleHint();

        // Keep domain the same, but change the path.
        String url2 =
                mShortDomain
                        + "/"
                        + TextUtils.join(
                                "", Collections.nCopies(NUMBER_OF_VISIBLE_CHARACTERS, "b"));
        mUrlBar.setText(url2);
        mUrlBar.setScrollState(UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        verify(mUrlBar, times(1)).calculateVisibleHint();
        String visibleHint = mUrlBar.getVisibleTextPrefixHint().toString();
        assertEquals(url2.substring(0, NUMBER_OF_VISIBLE_CHARACTERS + 1), visibleHint);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD)
    public void scrollToTLD_differentTLD_noVisibleHintCalculation() {
        doReturn(mLayout).when(mUrlBar).getLayout();
        doReturn(mPaint).when(mLayout).getPaint();

        measureAndLayoutUrlBar();
        // Url needs to be long enough to fill the entire url bar.
        String url =
                "www.a.com/"
                        + TextUtils.join(
                                "", Collections.nCopies(NUMBER_OF_VISIBLE_CHARACTERS, "a"));
        mUrlBar.setText(url);
        mUrlBar.setScrollState(UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        verify(mUrlBar, times(0)).calculateVisibleHint();

        // Change the domain, but keep the path the same.
        String url2 =
                "www.b.com/"
                        + TextUtils.join(
                                "", Collections.nCopies(NUMBER_OF_VISIBLE_CHARACTERS, "a"));
        mUrlBar.setText(url2);
        mUrlBar.setScrollState(UrlBar.ScrollType.SCROLL_TO_TLD, mShortDomain.length());
        verify(mUrlBar, times(0)).calculateVisibleHint();
        assertNull(mUrlBar.getVisibleTextPrefixHint());
    }

    @Test
    public void keyEvents_nonEnterActionDownKeyHandling() {
        var keysToCheck =
                List.of(
                        KeyEvent.KEYCODE_A,
                        KeyEvent.KEYCODE_TAB,
                        KeyEvent.KEYCODE_DPAD_UP,
                        KeyEvent.KEYCODE_DPAD_DOWN);

        var listener = mock(View.OnKeyListener.class);
        mUrlBar.setKeyDownListener(listener);

        for (int keyCode : keysToCheck) {
            var event = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);

            // Pre-IME Key Down, consumed: do not pass to IME.
            doReturn(true).when(listener).onKey(any(), anyInt(), any());
            assertTrue(mUrlBar.onKeyPreIme(keyCode, event));
            verify(listener).onKey(mUrlBar, keyCode, event);
            verify(mUrlBar, never()).super_onKeyPreIme(anyInt(), any());

            clearInvocations(listener, mUrlBar);

            // Pre-IME Key Down, not consumed: pass to IME.
            doReturn(false).when(listener).onKey(any(), anyInt(), any());
            doReturn(false).when(mUrlBar).super_onKeyPreIme(anyInt(), any());
            assertFalse(mUrlBar.onKeyPreIme(keyCode, event));
            verify(listener).onKey(mUrlBar, keyCode, event);
            verify(mUrlBar).super_onKeyPreIme(keyCode, event);

            clearInvocations(listener, mUrlBar);

            // Pre-IME Key Down, not consumed: return IME result.
            doReturn(true).when(mUrlBar).super_onKeyPreIme(anyInt(), any());
            assertTrue(mUrlBar.onKeyPreIme(keyCode, event));
            verify(mUrlBar).super_onKeyPreIme(keyCode, event);

            clearInvocations(listener, mUrlBar);

            // Post-IME Key Down: never passed to the listener.
            doReturn(false).when(mUrlBar).super_onKeyDown(anyInt(), any());
            assertFalse(mUrlBar.onKeyDown(keyCode, event));
            verifyNoMoreInteractions(listener);

            clearInvocations(listener, mUrlBar);

            // Post-IME Key Down: return IME result.
            doReturn(true).when(mUrlBar).super_onKeyDown(anyInt(), any());
            assertTrue(mUrlBar.onKeyDown(keyCode, event));
            verifyNoMoreInteractions(listener);

            clearInvocations(listener, mUrlBar);
        }
    }

    @Test
    public void keyEvents_enterActionDownKeyHandling() {
        var keysToCheck = List.of(KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER);

        var listener = mock(View.OnKeyListener.class);
        mUrlBar.setKeyDownListener(listener);

        for (int keyCode : keysToCheck) {
            var event = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);

            // Pre-IME Key Down: passed only to IME.
            doReturn(false).when(mUrlBar).super_onKeyPreIme(anyInt(), any());
            assertFalse(mUrlBar.onKeyPreIme(keyCode, event));
            verify(listener, never()).onKey(any(), anyInt(), any());
            verify(mUrlBar).super_onKeyPreIme(keyCode, event);

            clearInvocations(listener, mUrlBar);

            // Post-IME Key Down: consumed keys not passed to View.
            doReturn(true).when(listener).onKey(any(), anyInt(), any());
            assertTrue(mUrlBar.onKeyDown(keyCode, event));
            verify(listener).onKey(mUrlBar, keyCode, event);
            verify(mUrlBar, never()).super_onKeyDown(anyInt(), any());
            verifyNoMoreInteractions(listener);

            clearInvocations(listener, mUrlBar);

            // Post-IME Key Down: not consumed keys passed to View.
            doReturn(false).when(listener).onKey(any(), anyInt(), any());
            doReturn(true).when(mUrlBar).super_onKeyPreIme(anyInt(), any());
            assertTrue(mUrlBar.onKeyDown(keyCode, event));
            verify(listener).onKey(mUrlBar, keyCode, event);
            verify(mUrlBar).super_onKeyDown(keyCode, event);
            verifyNoMoreInteractions(listener);

            clearInvocations(listener, mUrlBar);
        }
    }

    @Test
    public void keyEvents_actionUpKeysBypassListenerCompletely() {
        var keysToCheck =
                List.of(
                        KeyEvent.KEYCODE_A,
                        KeyEvent.KEYCODE_TAB,
                        KeyEvent.KEYCODE_ENTER,
                        KeyEvent.KEYCODE_NUMPAD_ENTER,
                        KeyEvent.KEYCODE_DPAD_UP,
                        KeyEvent.KEYCODE_DPAD_DOWN);

        var listener = mock(View.OnKeyListener.class);
        mUrlBar.setKeyDownListener(listener);

        for (int keyCode : keysToCheck) {
            var event = new KeyEvent(KeyEvent.ACTION_UP, keyCode);

            // Pre-IME, not consumed by IME.
            doReturn(false).when(mUrlBar).super_onKeyPreIme(anyInt(), any());
            assertFalse(mUrlBar.onKeyPreIme(keyCode, event));
            verify(mUrlBar).super_onKeyPreIme(keyCode, event);
            verifyNoMoreInteractions(listener);

            clearInvocations(mUrlBar);

            // Pre-IME, consumed by IME.
            doReturn(true).when(mUrlBar).super_onKeyPreIme(anyInt(), any());
            assertTrue(mUrlBar.onKeyPreIme(keyCode, event));
            verify(mUrlBar).super_onKeyPreIme(keyCode, event);
            verifyNoMoreInteractions(listener);

            clearInvocations(mUrlBar);

            // Post-IME.
            assertFalse(mUrlBar.onKeyUp(keyCode, event));
            verifyNoMoreInteractions(listener);

            clearInvocations(mUrlBar);
        }
    }

    @Test
    public void horizontalFadingEdge_followsScrollWhenNotFocused() {
        // By default we show up unfocused.
        mUrlBar.setScrollX(0);
        assertTrue(mUrlBar.isHorizontalFadingEdgeEnabled());
        assertEquals(0.f, mUrlBar.getRightFadingEdgeStrength(), MathUtils.EPSILON);
        assertEquals(0.f, mUrlBar.getLeftFadingEdgeStrength(), MathUtils.EPSILON);

        // Scroll the view to the left. This should present fading edge now.
        mUrlBar.setScrollX(100);
        assertTrue(mUrlBar.isHorizontalFadingEdgeEnabled());
        assertEquals(0.f, mUrlBar.getRightFadingEdgeStrength(), MathUtils.EPSILON);
        assertEquals(1.f, mUrlBar.getLeftFadingEdgeStrength(), MathUtils.EPSILON);

        // Scroll back to initial position. Observe no fading.
        mUrlBar.setScrollX(0);
        assertTrue(mUrlBar.isHorizontalFadingEdgeEnabled());
        assertEquals(0.f, mUrlBar.getRightFadingEdgeStrength(), MathUtils.EPSILON);
        assertEquals(0.f, mUrlBar.getLeftFadingEdgeStrength(), MathUtils.EPSILON);
    }

    @Test
    public void horizontalFadingEdge_noFadeInWhenFocused() {
        measureAndLayoutUrlBar();
        mUrlBar.setScrollX(100);
        mUrlBar.onFocusChanged(true, View.LAYOUT_DIRECTION_LTR, null);
        assertFalse(mUrlBar.isHorizontalFadingEdgeEnabled());

        // NOTE: defocusing should restore fading edge.
        mUrlBar.onFocusChanged(false, View.LAYOUT_DIRECTION_LTR, null);
        assertTrue(mUrlBar.isHorizontalFadingEdgeEnabled());
    }

    /**
     * Simulate specific font metrics.
     *
     * @param fontActualHeight the desired actual difference between top and the bottom pixel ever
     *     drawn by the font
     */
    private void applyFontMetrics(float fontActualHeight) {
        mUrlBar.setTextSize(TypedValue.COMPLEX_UNIT_PX, FONT_HEIGHT_NOMINAL);
        doReturn((int) (FONT_HEIGHT_NOMINAL * LINE_HEIGHT_ELEGANT_FACTOR))
                .when(mUrlBar)
                .getLineHeight();
        // Respect the font height, but simulate that it's shifted 10px up.
        mFontMetrics.top = -10;
        mFontMetrics.bottom = fontActualHeight - 10;
        assertEquals(FONT_HEIGHT_NOMINAL, mUrlBar.getTextSize(), MathUtils.EPSILON);
        assertEquals(fontActualHeight, mUrlBar.getMaxHeightOfFont(), MathUtils.EPSILON);
    }

    /**
     * Compute the expected font height given the Url bar constraints.
     *
     * @param fontActualHeight the desired actual difference between top and the bottom pixel ever
     *     drawn by the font
     * @param urlBarHeight the usable area of the UrlBar that will accommodate the text
     */
    private float computeExpectedFontHeight(float fontActualHeight, int urlBarHeight) {
        float lineHeightScaleFactor = LINE_HEIGHT_ELEGANT_FACTOR;
        return FONT_HEIGHT_NOMINAL * (urlBarHeight / (fontActualHeight * lineHeightScaleFactor));
    }

    @Test
    public void enforceMaxTextHeight_shrinkTallFontToFit_withElegantText_noPadding() {
        measureAndLayoutUrlBar();
        applyFontMetrics(FONT_HEIGHT_ACTUAL_TALL);

        mUrlBar.setPaddingRelative(0, 0, 0, 0);
        mUrlBar.enforceMaxTextHeight();

        assertEquals(
                computeExpectedFontHeight(FONT_HEIGHT_ACTUAL_TALL, URL_BAR_HEIGHT),
                mUrlBar.getTextSize(),
                MathUtils.EPSILON);
    }

    @Test
    public void enforceMaxTextHeight_shrinkTallFontToFit_withElegantText_withPadding() {
        measureAndLayoutUrlBar();
        applyFontMetrics(FONT_HEIGHT_ACTUAL_TALL);

        mUrlBar.setPaddingRelative(0, 5, 0, 15);
        mUrlBar.enforceMaxTextHeight();

        assertEquals(
                computeExpectedFontHeight(FONT_HEIGHT_ACTUAL_TALL, URL_BAR_HEIGHT - 20),
                mUrlBar.getTextSize(),
                MathUtils.EPSILON);
    }

    @Test
    public void enforceMaxTextHeight_shrinkShortFontToFit_withElegantText_noPadding() {
        measureAndLayoutUrlBar();
        applyFontMetrics(FONT_HEIGHT_ACTUAL_SHORT);

        mUrlBar.setPaddingRelative(0, 0, 0, 0);
        mUrlBar.enforceMaxTextHeight();

        assertEquals(
                computeExpectedFontHeight(FONT_HEIGHT_ACTUAL_SHORT, URL_BAR_HEIGHT),
                mUrlBar.getTextSize(),
                MathUtils.EPSILON);
    }

    @Test
    public void enforceMaxTextHeight_shrinkShortFontToFit_withElegantText_withPadding() {
        measureAndLayoutUrlBar();
        applyFontMetrics(FONT_HEIGHT_ACTUAL_SHORT);

        mUrlBar.setPaddingRelative(0, 5, 0, 15);
        mUrlBar.enforceMaxTextHeight();

        assertEquals(
                computeExpectedFontHeight(FONT_HEIGHT_ACTUAL_SHORT, URL_BAR_HEIGHT - 20),
                mUrlBar.getTextSize(),
                MathUtils.EPSILON);
    }

    @Test
    public void layout_adjustFontSizeWithFixedHeight() {
        mUrlBar.setLayoutParams(new LayoutParams(123, 123));
        mUrlBar.layout(0, 0, 123, 123);
        verify(mUrlBar).post(mUrlBar.mEnforceMaxTextHeight);
    }

    @Test
    public void layout_adjustFontSizeLayoutRequested() {
        mUrlBar.setLayoutParams(new LayoutParams(123, 123));
        mUrlBar.layout(0, 0, 123, 123);
        verify(mUrlBar).post(mUrlBar.mEnforceMaxTextHeight);

        mUrlBar.forceLayout();
        mUrlBar.enforceMaxTextHeight();
        verify(mUrlBar, times(2)).post(mUrlBar.mEnforceMaxTextHeight);
    }

    @Test
    public void layout_fixedFontSizeWithWrappingHeight() {
        mUrlBar.setLayoutParams(new LayoutParams(123, LayoutParams.WRAP_CONTENT));
        mUrlBar.layout(0, 0, 123, 123);
        verify(mUrlBar, never()).post(mUrlBar.mEnforceMaxTextHeight);
    }

    @Test
    public void fixupTextDirection_unfocusedWithText() {
        mUrlBar.onFocusChanged(false, 0, null);
        mUrlBar.setText("test");
        assertEquals(View.TEXT_DIRECTION_LTR, mLastTextDirection);
        assertEquals(View.TEXT_ALIGNMENT_TEXT_START, mLastTextAlignment);
    }

    @Test
    public void fixupTextDirection_focusedWithText() {
        mUrlBar.onFocusChanged(true, 0, null);
        mUrlBar.setText("test");
        assertEquals(View.TEXT_DIRECTION_INHERIT, mLastTextDirection);
        assertEquals(View.TEXT_ALIGNMENT_TEXT_START, mLastTextAlignment);
    }

    @Test
    public void fixupTextDirection_unfocusedWithoutText() {
        mUrlBar.onFocusChanged(false, 0, null);
        mUrlBar.setText("");
        assertEquals(View.TEXT_DIRECTION_INHERIT, mLastTextDirection);
        assertEquals(View.TEXT_ALIGNMENT_VIEW_START, mLastTextAlignment);
    }

    @Test
    public void fixupTextDirection_focusedWithoutText() {
        mUrlBar.onFocusChanged(true, 0, null);
        mUrlBar.setText("");
        assertEquals(View.TEXT_DIRECTION_INHERIT, mLastTextDirection);
        assertEquals(View.TEXT_ALIGNMENT_VIEW_START, mLastTextAlignment);
    }

    @Test
    public void getTextWithAutocomplete_modelNotInitialized() {
        mUrlBar.setText("some autocomplete text");
        assertNull(mUrlBar.getModelForTesting());
        assertEquals("some autocomplete text", mUrlBar.getTextWithAutocomplete());
    }

    @Test
    public void getTextWithoutAutocomplete_modelNotInitialized() {
        mUrlBar.setText("some text");
        assertNull(mUrlBar.getModelForTesting());
        assertEquals("some text", mUrlBar.getTextWithoutAutocomplete());
    }

    @Test
    public void getTextWithAutocomplete_modelInitialized() {
        AutocompleteEditTextModelBase model = mock(AutocompleteEditTextModelBase.class);
        doReturn("model autocomplete text").when(model).getTextWithAutocomplete();
        mUrlBar.setText("user input");
        mUrlBar.setModelForTesting(model);
        assertEquals("model autocomplete text", mUrlBar.getTextWithAutocomplete());
    }

    @Test
    public void getTextWithoutAutocomplete_modelInitialized() {
        AutocompleteEditTextModelBase model = mock(AutocompleteEditTextModelBase.class);
        doReturn("model non-autocomplete text").when(model).getTextWithoutAutocomplete();
        mUrlBar.setText("user input");
        mUrlBar.setModelForTesting(model);
        assertEquals("model non-autocomplete text", mUrlBar.getTextWithoutAutocomplete());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.MULTILINE_EDIT_FIELD)
    public void setInputIsMultilineEligible() {
        mUrlBar.onFocusChanged(true, View.LAYOUT_DIRECTION_LTR, new Rect());
        mUrlBar.setInputIsMultilineEligible(true);
        assertEquals(UrlBar.MULTILINE_EDIT_MAX_LINES, mUrlBar.getMaxLines());
        assertFalse(mUrlBar.isSingleLine());
        assertFalse(mUrlBar.isHorizontallyScrollable());

        mUrlBar.setInputIsMultilineEligible(false);
        assertEquals(UrlBar.MULTILINE_EDIT_MAX_LINES, mUrlBar.getMaxLines());
        assertFalse(mUrlBar.isSingleLine());
        assertTrue(mUrlBar.isHorizontallyScrollable());

        // Defocused omnibox - never multiline
        mUrlBar.onFocusChanged(false, View.LAYOUT_DIRECTION_LTR, new Rect());
        mUrlBar.setInputIsMultilineEligible(true);
        assertEquals(1, mUrlBar.getMaxLines());
        assertTrue(mUrlBar.isSingleLine());
        assertTrue(mUrlBar.isHorizontallyScrollable());
    }

    @Test
    public void testTextWrappingCallback() {
        var callback = mock(Callback.class);
        mUrlBar.setUrlTextWrappingChangeListener(callback);
        doReturn(mLayout).when(mUrlBar).getLayout();

        mUrlBar.onFocusChanged(true, 0, null);
        measureAndLayoutUrlBar();

        // No single-line report (implied initial state).
        doReturn(1).when(mLayout).getLineCount();
        mUrlBar.onTextChanged("text", 0, 0, 4);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, never()).onResult(anyBoolean());
        clearInvocations(callback);

        // Report multi-line.
        doReturn(2).when(mLayout).getLineCount();
        mUrlBar.onTextChanged("longer text", 0, 0, 11);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback).onResult(true);
        clearInvocations(callback);

        // No repeated callbacks.
        mUrlBar.onTextChanged("longer text 2", 0, 0, 13);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, never()).onResult(anyBoolean());

        // Report single-line again.
        doReturn(1).when(mLayout).getLineCount();
        mUrlBar.onTextChanged("text", 0, 0, 4);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback).onResult(false);
        clearInvocations(callback);

        // No repeated callbacks.
        mUrlBar.onTextChanged("text 2", 0, 0, 6);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, never()).onResult(anyBoolean());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.URL_BAR_WITHOUT_LIGATURES)
    public void testUrlBarWithoutLigaturesEnabled() {
        assertEquals("liga=0, clig=0, calt=0, dlig=0", mUrlBar.getFontFeatureSettings());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.URL_BAR_WITHOUT_LIGATURES)
    public void testUrlBarWithoutLigaturesDisabled() {
        assertNull(mUrlBar.getFontFeatureSettings());
    }
}
