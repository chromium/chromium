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
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.SELECT_ALL_ON_FOCUS;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.TEXT_COLOR;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.view.View.OnLongClickListener;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.UrlBarViewBinderUnitTest.ShadowOmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link UrlBarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowOmniboxResourceProvider.class})
public class UrlBarViewBinderUnitTest {
    @Mock Callback<Boolean> mFocusChangeCallback;

    private Activity mActivity;
    PropertyModel mModel;
    UrlBarMediator mMediator;
    UrlBar mUrlBar;

    @Implements(OmniboxResourceProvider.class)
    static class ShadowOmniboxResourceProvider {
        @Implementation
        public static int getUrlBarPrimaryTextColor(
                Context context, @BrandedColorScheme int brandedColorScheme) {
            return Color.LTGRAY;
        }

        @Implementation
        public static int getUrlBarHintTextColor(
                Context context, @BrandedColorScheme int brandedColorScheme) {
            return Color.LTGRAY;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mModel = new PropertyModel(UrlBarProperties.ALL_KEYS);
        mMediator =
                new UrlBarMediator(
                        ContextUtils.getApplicationContext(), mModel, mFocusChangeCallback);
        mUrlBar = new UrlBarApi26(mActivity, null);
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
    public void testSetSelectAllOnFocus() {
        testSetSelectAllOnFocus(
                /* selectAllOnFocus= */ true,
                /* whileFocused= */ false,
                /* expectSelection= */ true);
    }

    @Test
    @SmallTest
    public void testSetSelectAllOnFocus_whileFocused() {
        testSetSelectAllOnFocus(
                /* selectAllOnFocus= */ true,
                /* whileFocused= */ true,
                /* expectSelection= */ false);
    }

    @Test
    @SmallTest
    public void testUnsetSelectAllOnFocus() {
        testSetSelectAllOnFocus(
                /* selectAllOnFocus= */ false,
                /* whileFocused= */ false,
                /* expectSelection= */ false);
    }

    @Test
    @SmallTest
    public void testUnsetSelectAllOnFocus_whileFocused() {
        testSetSelectAllOnFocus(
                /* selectAllOnFocus= */ false,
                /* whileFocused= */ true,
                /* expectSelection= */ false);
    }

    private void testSetSelectAllOnFocus(
            boolean selectAllOnFocus, boolean whileFocused, boolean expectSelection) {
        String text = "test";
        mUrlBar.setText(text);
        mUrlBar.setFocusable(true);
        Assert.assertFalse(mUrlBar.isFocused());
        Assert.assertFalse(mUrlBar.hasSelection());

        // Prevent the {@link mMediator} from clearing {@link text} on focus.
        mUrlBar.setOnFocusChangeListener(null);

        if (whileFocused) {
            mUrlBar.requestFocus();
            Assert.assertTrue(mUrlBar.isFocused());
        }

        mModel.set(SELECT_ALL_ON_FOCUS, selectAllOnFocus);

        if (!whileFocused) {
            mUrlBar.requestFocus();
            Assert.assertTrue(mUrlBar.isFocused());
        }

        Assert.assertEquals(expectSelection, mUrlBar.hasSelection());

        if (expectSelection) {
            Assert.assertEquals(0, mUrlBar.getSelectionStart());
            Assert.assertEquals(text.length(), mUrlBar.getSelectionEnd());
        }
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
        mModel.set(HINT_TEXT, R.string.hub_search_empty_hint);
        Assert.assertEquals(mActivity.getString(R.string.hub_search_empty_hint), mUrlBar.getHint());
        mModel.set(HINT_TEXT, R.string.hub_search_empty_hint_incognito);
        Assert.assertEquals(
                mActivity.getString(R.string.hub_search_empty_hint_incognito), mUrlBar.getHint());
    }
}
