// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.browser.omnibox.UrlBarProperties.HINT_TEXT_COLOR;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.TEXT_COLOR;
import static org.chromium.chrome.browser.omnibox.UrlBarProperties.TYPEFACE;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;

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

/**
 * Unit tests for {@link UrlBarViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowOmniboxResourceProvider.class})
public class UrlBarViewBinderUnitTest {
    @Mock
    Callback<Boolean> mFocusChangeCallback;

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
        mMediator = new UrlBarMediator(
                ContextUtils.getApplicationContext(), mModel, mFocusChangeCallback);
        mUrlBar = new UrlBarApi26(mActivity, null);
        PropertyModelChangeProcessor.create(mModel, mUrlBar, UrlBarViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testSetTextTypeface() {
        Typeface expectTypeFace = Typeface.create("google-sans-medium", Typeface.NORMAL);
        mModel.set(TYPEFACE, expectTypeFace);
        Assert.assertEquals(expectTypeFace, mUrlBar.getTypeface());
        Typeface newExpectTypeFace = Typeface.defaultFromStyle(Typeface.NORMAL);
        mModel.set(TYPEFACE, newExpectTypeFace);
        Assert.assertEquals(newExpectTypeFace, mUrlBar.getTypeface());
        Assert.assertNotEquals(newExpectTypeFace, expectTypeFace);
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
}
