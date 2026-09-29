// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.omnibox.UrlBarProperties.HINT_TEXT;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.HINT_TEXT_COLOR;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.TEXT_COLOR;

import android.content.Context;
import android.graphics.Color;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnLongClickListener;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams;

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
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link UrlBarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlBarViewBinderUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private View.OnKeyListener mOnKeyListener;
    @Mock private OnLongClickListener mOnLongClickListener;
    @Mock private Runnable mRunnable;

    private Context mContext;
    private PropertyModel mModel;
    private UrlBar mUrlBar;
    private final ConstraintLayout.LayoutParams mUrlBarLayoutParams = new LayoutParams(0, 100);

    @Before
    public void setUp() {
        OmniboxResourceProvider.setUrlBarPrimaryTextColorForTesting(Color.LTGRAY);
        OmniboxResourceProvider.setUrlBarHintTextColorForTesting(Color.LTGRAY);
        mContext = ContextUtils.getApplicationContext();

        mModel =
                new PropertyModel.Builder(UrlBarProperties.ALL_KEYS)
                        .with(UrlBarProperties.USE_SMALL_TEXT, false)
                        .with(UrlBarProperties.SHOW_HINT_TEXT, true)
                        .with(UrlBarProperties.LONG_CLICK_LISTENER, mOnLongClickListener)
                        .with(UrlBarProperties.KEY_DOWN_LISTENER, mOnKeyListener)
                        .build();
        mUrlBar = new UrlBarApi26(mContext, null);
        mUrlBar.setLayoutParams(mUrlBarLayoutParams);
        PropertyModelChangeProcessor.create(mModel, mUrlBar, UrlBarViewBinder::bind);
    }

    @Test
    public void testSetHintTextColor() {
        int expectColor = Color.RED;
        mModel.set(HINT_TEXT_COLOR, expectColor);
        assertEquals(expectColor, mUrlBar.getHintTextColors().getDefaultColor());
        int newExpectColor = Color.GREEN;
        mModel.set(HINT_TEXT_COLOR, newExpectColor);
        assertEquals(newExpectColor, mUrlBar.getHintTextColors().getDefaultColor());
    }

    @Test
    public void testSetTextColor() {
        int expectColor = Color.RED;
        mModel.set(TEXT_COLOR, expectColor);
        assertEquals(expectColor, mUrlBar.getTextColors().getDefaultColor());
        int newExpectColor = Color.GREEN;
        mModel.set(TEXT_COLOR, newExpectColor);
        assertEquals(newExpectColor, mUrlBar.getTextColors().getDefaultColor());
    }

    @Test
    public void testOnLongClick() {
        doReturn(true).when(mOnLongClickListener).onLongClick(mUrlBar);
        assertTrue(mUrlBar.performLongClick());
        verify(mOnLongClickListener).onLongClick(mUrlBar);
    }

    @Test
    public void testKeyDownListener() {
        KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
        mUrlBar.onKeyDown(KeyEvent.KEYCODE_ENTER, event);
        verify(mOnKeyListener).onKey(mUrlBar, KeyEvent.KEYCODE_ENTER, event);
    }

    @Test
    public void testSetHintText() {
        mModel.set(HINT_TEXT, "Hint Text");
        assertEquals("Hint Text", mUrlBar.getHint());
        mModel.set(HINT_TEXT, "Different Hint Text");
        assertEquals("Different Hint Text", mUrlBar.getHint());

        mModel.set(UrlBarProperties.USE_SMALL_TEXT, true);
        assertNull(mUrlBar.getHint());
        mModel.set(HINT_TEXT, "Hint Text");
        assertNull(mUrlBar.getHint());
        mModel.set(UrlBarProperties.USE_SMALL_TEXT, false);
        assertEquals("Hint Text", mUrlBar.getHint());

        mModel.set(UrlBarProperties.SHOW_HINT_TEXT, false);
        assertNull(mUrlBar.getHint());
    }

    @Test
    public void testTextSize() {
        mUrlBar.setPaddingRelative(13, 0, 17, 0);
        int normalPadding =
                mContext.getResources().getDimensionPixelSize(R.dimen.url_bar_vertical_padding);
        int smallPadding = 0;

        mModel.set(UrlBarProperties.USE_SMALL_TEXT, true);
        assertEquals(LayoutParams.WRAP_CONTENT, mUrlBarLayoutParams.width);
        assertEquals(smallPadding, mUrlBar.getPaddingBottom());
        assertEquals(smallPadding, mUrlBar.getPaddingTop());
        assertEquals(13, mUrlBar.getPaddingStart());
        assertEquals(17, mUrlBar.getPaddingEnd());

        mModel.set(UrlBarProperties.USE_SMALL_TEXT, false);
        assertEquals(LayoutParams.MATCH_CONSTRAINT, mUrlBarLayoutParams.width);
        assertEquals(normalPadding, mUrlBar.getPaddingBottom());
        assertEquals(normalPadding, mUrlBar.getPaddingTop());
        assertEquals(13, mUrlBar.getPaddingStart());
        assertEquals(17, mUrlBar.getPaddingEnd());
    }

    @Test
    public void testSetAllowMultilineInput() {
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, true);
        mUrlBar.onFocusChanged(
                /* focused= */ true, View.FOCUS_DOWN, /* previouslyFocusedRect= */ null);
        mUrlBar.setInputIsMultilineEligible(true);
        assertFalse(mUrlBar.isHorizontallyScrollable());

        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, false);
        assertTrue(mUrlBar.isHorizontallyScrollable());

        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, true);
        assertFalse(mUrlBar.isHorizontallyScrollable());
    }

    @Test
    public void testSetManageSearchEnginesCallback() {
        mModel.set(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK, mRunnable);
        assertEquals(mRunnable, mUrlBar.getManageSearchEnginesCallback());
    }

    @Test
    public void testTextState_reverseSelection() {
        mUrlBar.setAllowFocus(true);
        mUrlBar.requestFocus();

        UrlBarTextState state =
                new UrlBarTextState(
                        "1234567890",
                        "1234567890",
                        ScrollType.NO_SCROLL,
                        /* scrollToIndex= */ 0,
                        new TextSelection(10, 0),
                        /* originChanged= */ false);

        mModel.set(UrlBarProperties.TEXT_STATE, state);

        assertEquals(10, mUrlBar.getSelectionStart());
        assertEquals(0, mUrlBar.getSelectionEnd());
    }
}
