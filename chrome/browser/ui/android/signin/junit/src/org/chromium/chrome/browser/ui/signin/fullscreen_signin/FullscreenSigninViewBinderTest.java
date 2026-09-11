// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.test.filters.SmallTest;

import com.airbnb.lottie.LottieAnimationView;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link FullscreenSigninViewBinder} and {@link FullscreenSigninProperties}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenSigninViewBinderTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private FullscreenSigninView mView;
    @Mock private LottieAnimationView mAnimationView;

    @Test
    @SmallTest
    public void testCreateModel_animationInitiallyHidden() {
        when(mView.getAnimationView()).thenReturn(mAnimationView);
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
        verify(mAnimationView).setVisibility(View.GONE);
    }

    @Test
    @SmallTest
    public void testBindShowAnimation_visible() {
        when(mView.getAnimationView()).thenReturn(mAnimationView);
        PropertyModel model =
                new PropertyModel.Builder(FullscreenSigninProperties.ALL_KEYS)
                        .with(FullscreenSigninProperties.SHOW_ANIMATION, true)
                        .build();
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.SHOW_ANIMATION);
        verify(mAnimationView).setVisibility(View.VISIBLE);
    }

    @Test
    @SmallTest
    public void testBindShowAnimation_gone() {
        when(mView.getAnimationView()).thenReturn(mAnimationView);
        PropertyModel model =
                new PropertyModel.Builder(FullscreenSigninProperties.ALL_KEYS)
                        .with(FullscreenSigninProperties.SHOW_ANIMATION, false)
                        .build();
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.SHOW_ANIMATION);
        verify(mAnimationView).setVisibility(View.GONE);
    }

    @Test
    @SmallTest
    public void testBindProfilePicture() {
        Drawable drawable = mock(Drawable.class);
        ImageView iconView = mock(ImageView.class);
        when(mView.getIcon()).thenReturn(iconView);
        PropertyModel model =
                new PropertyModel.Builder(FullscreenSigninProperties.ALL_KEYS)
                        .with(FullscreenSigninProperties.PROFILE_PICTURE, drawable)
                        .build();
        FullscreenSigninViewBinder.bind(model, mView, FullscreenSigninProperties.PROFILE_PICTURE);
        verify(iconView).setImageDrawable(drawable);
    }
}
