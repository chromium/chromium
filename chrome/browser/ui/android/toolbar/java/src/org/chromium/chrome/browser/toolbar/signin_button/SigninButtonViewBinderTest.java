// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static org.junit.Assert.assertEquals;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.view.LayoutInflater;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ValueUtils;

/** Unit tests for {@link SigninButtonViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninButtonViewBinderTest {
    private static final float FLOAT_EPSILON = 1e-4f;

    private SigninButtonView mView;
    private ListMenuButton mAvatarButton;
    private PropertyModel mModel;
    private float mDisabledAlpha;

    @Before
    public void setUp() {
        AppCompatActivity activity =
                Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mView =
                (SigninButtonView)
                        LayoutInflater.from(activity).inflate(R.layout.signin_button_view, null);
        mAvatarButton = mView.getAvatarButton();
        mDisabledAlpha =
                ValueUtils.getFloat(
                        mAvatarButton.getResources(),
                        org.chromium.ui.R.dimen.default_disabled_alpha);

        mModel =
                new PropertyModel.Builder(SigninButtonProperties.ALL_KEYS)
                        .with(SigninButtonProperties.IS_ENABLED, true)
                        .with(SigninButtonProperties.AVATAR_TINT, null)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, SigninButtonViewBinder::bind);
    }

    @Test
    public void testSignedInAvatarDimsWhenDisabled() {
        assertEquals(1.0f, mAvatarButton.getAlpha(), FLOAT_EPSILON);

        mModel.set(SigninButtonProperties.IS_ENABLED, false);
        assertEquals(mDisabledAlpha, mAvatarButton.getAlpha(), FLOAT_EPSILON);

        mModel.set(SigninButtonProperties.IS_ENABLED, true);
        assertEquals(1.0f, mAvatarButton.getAlpha(), FLOAT_EPSILON);
    }

    @Test
    public void testTintedAvatarNotDoubleDimmedWhenDisabled() {
        ColorStateList tint = ColorStateList.valueOf(Color.GRAY);
        mModel.set(SigninButtonProperties.AVATAR_TINT, tint);
        mModel.set(SigninButtonProperties.IS_ENABLED, false);
        assertEquals(1.0f, mAvatarButton.getAlpha(), FLOAT_EPSILON);

        // Switching to an untinted (signed-in) avatar while disabled applies view alpha dimming.
        mModel.set(SigninButtonProperties.AVATAR_TINT, null);
        assertEquals(mDisabledAlpha, mAvatarButton.getAlpha(), FLOAT_EPSILON);

        // Switching back to a tinted avatar restores view alpha so tint handles dimming alone.
        mModel.set(SigninButtonProperties.AVATAR_TINT, tint);
        assertEquals(1.0f, mAvatarButton.getAlpha(), FLOAT_EPSILON);
    }
}
