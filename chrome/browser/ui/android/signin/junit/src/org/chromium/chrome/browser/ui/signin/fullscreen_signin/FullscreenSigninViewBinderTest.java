// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link FullscreenSigninViewBinder} and {@link FullscreenSigninProperties}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenSigninViewBinderTest {
    private FullscreenSigninView mView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView =
                (FullscreenSigninView)
                        activity.getLayoutInflater()
                                .inflate(R.layout.fullscreen_signin_portrait_view, null);
    }

    @Test
    public void testCreateModel_animationInitiallyHidden() {
        mView.getAnimationView().setVisibility(View.VISIBLE);
        PropertyModel model =
                FullscreenSigninProperties.createModel(
                        () -> {},
                        () -> {},
                        () -> {},
                        true,
                        0,
                        "Title",
                        "Subtitle",
                        "Dismiss",
                        false);
        assertFalse(model.get(FullscreenSigninProperties.SHOW_ANIMATION));
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.SHOW_ANIMATION);
        assertEquals(View.GONE, mView.getAnimationView().getVisibility());
    }

    @Test
    public void testBindShowAnimation_visible() {
        mView.getAnimationView().setVisibility(View.GONE);
        PropertyModel model =
                new PropertyModel.Builder(FullscreenSigninProperties.ALL_KEYS)
                        .with(FullscreenSigninProperties.SHOW_ANIMATION, true)
                        .build();
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.SHOW_ANIMATION);
        assertEquals(View.VISIBLE, mView.getAnimationView().getVisibility());
    }

    @Test
    public void testBindShowAnimation_gone() {
        mView.getAnimationView().setVisibility(View.VISIBLE);
        PropertyModel model =
                new PropertyModel.Builder(FullscreenSigninProperties.ALL_KEYS)
                        .with(FullscreenSigninProperties.SHOW_ANIMATION, false)
                        .build();
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.SHOW_ANIMATION);
        assertEquals(View.GONE, mView.getAnimationView().getVisibility());
    }

    @Test
    public void testBindProfilePicture() {
        Drawable drawable = new ColorDrawable(Color.RED);
        PropertyModel model =
                new PropertyModel.Builder(FullscreenSigninProperties.ALL_KEYS)
                        .with(FullscreenSigninProperties.PROFILE_PICTURE, drawable)
                        .build();
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.PROFILE_PICTURE);
        assertSame(drawable, mView.getIcon().getDrawable());
    }
}
