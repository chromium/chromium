// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.omnibox.UrlBarProperties.HINT_TEXT;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.HINT_TEXT_COLOR;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.TEXT_COLOR;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.View.OnLongClickListener;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link UrlBarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UrlBarViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Callback<Boolean> mFocusChangeCallback;

    private Activity mActivity;
    PropertyModel mModel;
    UrlBarMediator mMediator;
    UrlBar mUrlBar;
    ConstraintLayout.LayoutParams mUrlBarLayoutParams = new LayoutParams(0, 100);

    @Before
    public void setUp() {
        OmniboxResourceProvider.setUrlBarPrimaryTextColorForTesting(Color.LTGRAY);
        OmniboxResourceProvider.setUrlBarHintTextColorForTesting(Color.LTGRAY);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mModel = new PropertyModel(UrlBarProperties.ALL_KEYS);
        mModel.set(UrlBarProperties.USE_SMALL_TEXT, false);
        mMediator =
                new UrlBarMediator(
                        ContextUtils.getApplicationContext(),
                        mModel,
                        mFocusChangeCallback,
                        /* textChangeListener= */ null,
                        /* richTextChangeListener= */ null,
                        /* keyDownListener= */ null);
        mUrlBar = new UrlBarApi26(mActivity, null);
        mUrlBar.setLayoutParams(mUrlBarLayoutParams);
        PropertyModelChangeProcessor.create(mModel, mUrlBar, UrlBarViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testSetHintTextColor() {
        int expectColor = Color.RED;
        mModel.set(HINT_TEXT_COLOR, expectColor);
        assertEquals(expectColor, mUrlBar.getHintTextColors().getDefaultColor());
        int newExpectColor = Color.GREEN;
        mModel.set(HINT_TEXT_COLOR, newExpectColor);
        assertEquals(newExpectColor, mUrlBar.getHintTextColors().getDefaultColor());
    }

    @Test
    @SmallTest
    public void testSetTextColor() {
        int expectColor = Color.RED;
        mModel.set(TEXT_COLOR, expectColor);
        assertEquals(expectColor, mUrlBar.getTextColors().getDefaultColor());
        int newExpectColor = Color.GREEN;
        mModel.set(TEXT_COLOR, newExpectColor);
        assertEquals(newExpectColor, mUrlBar.getTextColors().getDefaultColor());
    }

    @Test
    @SmallTest
    public void testOnLongClick() {
        OnLongClickListener longClickListener = mock(OnLongClickListener.class);
        doReturn(true).when(longClickListener).onLongClick(any());

        mModel.set(UrlBarProperties.LONG_CLICK_LISTENER, longClickListener);
        mUrlBar.performLongClick();
        verify(longClickListener).onLongClick(any());
    }

    @Test
    @SmallTest
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
    @SmallTest
    public void testTextSize() {
        mUrlBar.setPaddingRelative(13, 0, 17, 0);
        int normalPadding =
                mActivity.getResources().getDimensionPixelSize(R.dimen.url_bar_vertical_padding);
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
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.MULTILINE_EDIT_FIELD)
    public void testSetAllowMultilineInput() {
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, true);
        mUrlBar.onFocusChanged(true, View.FOCUS_DOWN, null);
        mUrlBar.setInputIsMultilineEligible(true);
        assertFalse(mUrlBar.isHorizontallyScrollable());

        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, false);
        assertTrue(mUrlBar.isHorizontallyScrollable());
    }

    @Test
    @SmallTest
    public void testSetManageSearchEnginesCallback() {
        Runnable mockCallback = mock(Runnable.class);
        mModel.set(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK, mockCallback);
        assertEquals(mockCallback, mUrlBar.getManageSearchEnginesCallbackForTesting());
    }

    @Test
    @SmallTest
    public void testTextState_reverseSelection() {
        UrlBar mockView = mock(UrlBar.class);
        android.text.Editable editable = mock(android.text.Editable.class);
        doReturn(10).when(editable).length();
        doReturn(editable).when(mockView).getText();
        doReturn(true).when(mockView).hasFocus();

        UrlBarTextState state =
                new UrlBarTextState(
                        "1234567890",
                        "1234567890",
                        ScrollType.NO_SCROLL,
                        0,
                        new TextSelection(10, 0),
                        false);

        mModel.set(UrlBarProperties.TEXT_STATE, state);
        UrlBarViewBinder.bind(mModel, mockView, UrlBarProperties.TEXT_STATE);

        verify(mockView).setSelection(10, 0);
    }
}
