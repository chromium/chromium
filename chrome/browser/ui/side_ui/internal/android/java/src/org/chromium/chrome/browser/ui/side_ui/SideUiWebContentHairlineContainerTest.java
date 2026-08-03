// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SideUiWebContentHairlineContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SideUiWebContentHairlineContainerTest {

    private TestActivity mTestActivity;
    private SideUiWebContentHairlineContainer mContainer;

    private ImageView mTopHairline;
    private ImageView mLeftHairline;
    private ImageView mRightHairline;

    private ImageView mTopLeftRoundedCorner;
    private ImageView mTopRightRoundedCorner;

    @Before
    public void setUp() {
        mTestActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mContainer =
                (SideUiWebContentHairlineContainer)
                        LayoutInflater.from(mTestActivity)
                                .inflate(
                                        R.layout.side_ui_web_content_hairline_container,
                                        /* root= */ null);

        mTopHairline = mContainer.getTopHairline();
        mLeftHairline = mContainer.getLeftHairline();
        mRightHairline = mContainer.getRightHairline();

        mTopLeftRoundedCorner = mContainer.getTopLeftRoundedCorner();
        mTopRightRoundedCorner = mContainer.getTopRightRoundedCorner();
    }

    @Test
    public void testSetIncognitoState_True() {
        mContainer.setIncognitoState(/* isIncognito= */ true);

        ColorStateList expectedTint =
                ColorStateList.valueOf(
                        mTestActivity.getColor(
                                R.color.web_content_hairline_divider_color_incognito));
        assertEquals("Unexpected tint.", expectedTint, mTopHairline.getImageTintList());
        assertEquals("Unexpected tint.", expectedTint, mLeftHairline.getImageTintList());
        assertEquals("Unexpected tint.", expectedTint, mRightHairline.getImageTintList());

        int topLeftResId =
                Shadows.shadowOf(mTopLeftRoundedCorner.getDrawable()).getCreatedFromResId();
        int topRightResId =
                Shadows.shadowOf(mTopRightRoundedCorner.getDrawable()).getCreatedFromResId();
        assertEquals("Unexpected resId.", R.drawable.rounded_corner_left_incognito, topLeftResId);
        assertEquals("Unexpected resId.", R.drawable.rounded_corner_right_incognito, topRightResId);
    }

    @Test
    public void testSetIncognitoState_False() {
        mContainer.setIncognitoState(/* isIncognito= */ false);

        assertNull("Expected null tint.", mTopHairline.getImageTintList());
        assertNull("Expected null tint.", mLeftHairline.getImageTintList());
        assertNull("Expected null tint.", mRightHairline.getImageTintList());

        int topLeftResId =
                Shadows.shadowOf(mTopLeftRoundedCorner.getDrawable()).getCreatedFromResId();
        int topRightResId =
                Shadows.shadowOf(mTopRightRoundedCorner.getDrawable()).getCreatedFromResId();
        assertEquals("Unexpected resId.", R.drawable.rounded_corner_left, topLeftResId);
        assertEquals("Unexpected resId.", R.drawable.rounded_corner_right, topRightResId);
    }
}
