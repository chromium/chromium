// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

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

import org.junit.Assert;
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
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.omnibox.OmniboxFeatureList;
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
                        ContextUtils.getApplicationContext(), mModel, mFocusChangeCallback);
        mUrlBar = new UrlBarApi26(mActivity, null);
        mUrlBar.setLayoutParams(mUrlBarLayoutParams);
        PropertyModelChangeProcessor.create(mModel, mUrlBar, UrlBarViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testSetHintTextColor() {
        int expectColor = Color.RED;
        mModel.set(HINT_TEXT_COLOR, expectColor);
        Assert.assertEquals(expectColor, mUrlBar.getHintTextColors().getDefaultColor());
        int newExpectColor = Color.GREEN;
        mModel.set(HINT_TEXT_COLOR, newExpectColor);
        Assert.assertEquals(newExpectColor, mUrlBar.getHintTextColors().getDefaultColor());
    }

    @Test
    @SmallTest
    public void testSetTextColor() {
        int expectColor = Color.RED;
        mModel.set(TEXT_COLOR, expectColor);
        Assert.assertEquals(expectColor, mUrlBar.getTextColors().getDefaultColor());
        int newExpectColor = Color.GREEN;
        mModel.set(TEXT_COLOR, newExpectColor);
        Assert.assertEquals(newExpectColor, mUrlBar.getTextColors().getDefaultColor());
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
        Assert.assertEquals("Hint Text", mUrlBar.getHint());
        mModel.set(HINT_TEXT, "Different Hint Text");
        Assert.assertEquals("Different Hint Text", mUrlBar.getHint());

        mModel.set(UrlBarProperties.USE_SMALL_TEXT, true);
        Assert.assertNull(mUrlBar.getHint());
        mModel.set(HINT_TEXT, "Hint Text");
        Assert.assertNull(mUrlBar.getHint());
        mModel.set(UrlBarProperties.USE_SMALL_TEXT, false);
        Assert.assertEquals("Hint Text", mUrlBar.getHint());

        mModel.set(UrlBarProperties.SHOW_HINT_TEXT, false);
        Assert.assertNull(mUrlBar.getHint());
    }

    @Test
    @SmallTest
    public void testTextSize() {
        mUrlBar.setPaddingRelative(13, 0, 17, 0);
        int normalPadding =
                mActivity.getResources().getDimensionPixelSize(R.dimen.url_bar_vertical_padding);
        int smallPadding = 0;

        mModel.set(UrlBarProperties.USE_SMALL_TEXT, true);
        Assert.assertEquals(LayoutParams.WRAP_CONTENT, mUrlBarLayoutParams.width);
        Assert.assertEquals(smallPadding, mUrlBar.getPaddingBottom());
        Assert.assertEquals(smallPadding, mUrlBar.getPaddingTop());
        Assert.assertEquals(13, mUrlBar.getPaddingStart());
        Assert.assertEquals(17, mUrlBar.getPaddingEnd());

        mModel.set(UrlBarProperties.USE_SMALL_TEXT, false);
        Assert.assertEquals(LayoutParams.MATCH_CONSTRAINT, mUrlBarLayoutParams.width);
        Assert.assertEquals(normalPadding, mUrlBar.getPaddingBottom());
        Assert.assertEquals(normalPadding, mUrlBar.getPaddingTop());
        Assert.assertEquals(13, mUrlBar.getPaddingStart());
        Assert.assertEquals(17, mUrlBar.getPaddingEnd());
    }

    @Test
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.MULTILINE_EDIT_FIELD)
    public void testSetAllowMultilineInput() {
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, true);
        mUrlBar.onFocusChanged(true, View.FOCUS_DOWN, null);
        Assert.assertFalse(mUrlBar.isSingleLine());
        Assert.assertEquals(UrlBar.MULTILINE_EDIT_MAX_LINES, mUrlBar.getMaxLines());

        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, false);
        mUrlBar.onFocusChanged(true, View.FOCUS_DOWN, null);
        Assert.assertTrue(mUrlBar.isSingleLine());
        Assert.assertEquals(1, mUrlBar.getMaxLines());
    }
}
