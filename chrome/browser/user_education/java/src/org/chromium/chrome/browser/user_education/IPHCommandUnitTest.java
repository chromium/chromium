// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.user_education;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.content.res.Resources;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the IPHCommand. */
@RunWith(BaseRobolectricTestRunner.class)
public class IPHCommandUnitTest {
    private static final String FEATURE_NAME = "Builder Unit Test";
    private static final int CONTENT_STRING_RES = 1234;
    private static final int ACCESSIBILITY_STRING_RES = 4321;
    private static final int DEFAULT_INSET_BOTTOM = 2468;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Resources mResources;
    private IPHCommandBuilder mBuilder;

    @Before
    public void setUp() {
        doReturn(DEFAULT_INSET_BOTTOM)
                .when(mResources)
                .getDimensionPixelOffset(R.dimen.iph_text_bubble_menu_anchor_y_inset);

        // Facilitates creation of IPHCommand objects used for testing.
        mBuilder =
                new IPHCommandBuilder(
                        mResources, FEATURE_NAME, CONTENT_STRING_RES, ACCESSIBILITY_STRING_RES);
    }

    @Test
    public void fetchFromResources_useFallbackInsetsWhenNotSet() {
        var cmd = mBuilder.setInsetRect(null).build();
        assertEquals(null, cmd.insetRect);

        cmd.fetchFromResources();
        assertEquals(new Rect(0, 0, 0, DEFAULT_INSET_BOTTOM), cmd.insetRect);
    }

    @Test
    public void fetchFromResources_useSuppliedInsets() {
        var rect = new Rect(10, 20, 30, 40);
        var cmd = mBuilder.setInsetRect(rect).build();
        assertEquals(rect, cmd.insetRect);

        cmd.fetchFromResources();
        assertEquals(rect, cmd.insetRect);
    }
}
