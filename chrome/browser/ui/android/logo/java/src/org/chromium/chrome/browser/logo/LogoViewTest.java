// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.ObjectAnimator;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;

/** Instrumentation tests for {@link LogoView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LogoViewTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock public TemplateUrlService mTemplateUrlService;
    @Mock public LogoView.ClickHandler mLogoClickHandler;

    private static final String LOGO_URL = "https://www.google.com";
    private static final String ANIMATED_LOGO_URL =
            "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json";
    private static final String DARK_ANIMATED_LOGO_URL =
            "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4_dark.json";
    private static final String ALT_TEXT = "Hello World!";

    private LogoView mView;
    private Bitmap mBitmap;
    private Bitmap mDarkBitmap;

    @Before
    public void setup() {
        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);
        mDarkBitmap = Bitmap.createBitmap(1, 1, Config.ARGB_8888);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            FrameLayout parent = new FrameLayout(activity);
                            mView = new LogoView(activity, null);
                            parent.addView(
                                    mView,
                                    new LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

                            MarginLayoutParams params =
                                    new MarginLayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                            activity.setContentView(parent, params);
                        });
    }

    @Test
    public void testDefaultLogoView() {
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogoDrawable(
                mView.getContext().getDrawable(R.drawable.ic_google_logo));
        mView.maybeShowDefaultLogoDrawable();
        mView.endAnimationsForTesting();

        Assert.assertFalse("Default logo should not be clickable.", mView.isClickable());
        Assert.assertFalse("Default logo should not be focusable.", mView.isFocusable());
        Assert.assertTrue(
                "Default logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithUrl() {
        Logo logo =
                new Logo(
                        /* image= */ mBitmap,
                        /* darkImage= */ mDarkBitmap,
                        /* onClickUrl= */ LOGO_URL,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertTrue("Logo with URL should be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with URL should be focusable.", mView.isFocusable());
        Assert.assertTrue(
                "Logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithAnimatedUrl() {
        Logo logo =
                new Logo(
                        /* image= */ mBitmap,
                        /* darkImage= */ mDarkBitmap,
                        /* onClickUrl= */ null,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ ANIMATED_LOGO_URL,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertTrue("Logo with animated URL should be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with animated URL should be focusable.", mView.isFocusable());
        Assert.assertTrue(
                "Logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithUrl_Clicked() {
        mView.setClickHandler(mLogoClickHandler);
        Logo logo =
                new Logo(
                        /* image= */ mBitmap,
                        /* darkImage= */ mDarkBitmap,
                        /* onClickUrl= */ LOGO_URL,
                        /* altText= */ null,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        mView.performClick();
        verify(mLogoClickHandler, times(1)).onLogoClicked(false);
    }

    @Test
    public void testLogoView_WithAltText() {
        Logo logo =
                new Logo(
                        /* image= */ mBitmap,
                        /* darkImage= */ mDarkBitmap,
                        /* onClickUrl= */ null,
                        /* altText= */ ALT_TEXT,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertFalse("Logo without URL should not be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with alt text should be focusable.", mView.isFocusable());
        Assert.assertFalse(
                "Logo should have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    @MediumTest
    public void testDoodleAnimation() {
        // Test default google logo drawable.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogoDrawable(
                mView.getContext().getDrawable(R.drawable.ic_google_logo));

        testDoodleAnimationImpl();
    }

    private void testDoodleAnimationImpl() {
        Resources res = mView.getResources();
        int logoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int logoTopMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int doodleHeight = LogoUtils.getDoodleHeightInTabletSplitScreen(res);
        int doodleTopMargin = LogoUtils.getTopMarginForDoodle(res);
        MarginLayoutParams logoLayoutParams = (MarginLayoutParams) mView.getLayoutParams();

        mView.maybeShowDefaultLogoDrawable();
        mView.endAnimationsForTesting();
        Assert.assertEquals(logoHeight, logoLayoutParams.height);
        Assert.assertEquals(logoTopMargin, logoLayoutParams.topMargin);

        // Test doodle animation.
        Logo logo =
                new Logo(
                        /* image= */ mBitmap,
                        /* darkImage= */ mDarkBitmap,
                        /* onClickUrl= */ null,
                        /* altText= */ ALT_TEXT,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null);
        mView.updateLogo(logo);

        // With TransitionManager, layout params are updated immediately.
        Assert.assertEquals(doodleHeight, logoLayoutParams.height);
        Assert.assertEquals(doodleTopMargin, logoLayoutParams.topMargin);

        ObjectAnimator fadeAnimation = mView.getFadeAnimationForTesting();
        Assert.assertNotNull(fadeAnimation);

        fadeAnimation.pause();

        fadeAnimation.setCurrentFraction(0);
        Assert.assertEquals(1.0f, mView.getAlpha(), 0.01f);

        fadeAnimation.setCurrentFraction(0.25f);
        Assert.assertEquals(0.5f, mView.getAlpha(), 0.01f);

        fadeAnimation.setCurrentFraction(0.5f);
        Assert.assertEquals(0.0f, mView.getAlpha(), 0.01f);

        fadeAnimation.setCurrentFraction(0.75f);
        Assert.assertEquals(0.5f, mView.getAlpha(), 0.01f);

        fadeAnimation.setCurrentFraction(1);
        Assert.assertEquals(1.0f, mView.getAlpha(), 0.01f);
    }

    @Test
    public void testSetLogoTopMargin() {
        MarginLayoutParams params = (MarginLayoutParams) mView.getLayoutParams();
        mView.setLogoTopMargin(100);
        Assert.assertEquals(100, params.topMargin);
    }

    @Test
    public void testSetLogoHeight() {
        MarginLayoutParams params = (MarginLayoutParams) mView.getLayoutParams();
        mView.setLogoHeight(200);
        Assert.assertEquals(200, params.height);
    }

    @Test
    public void testScaleTypeSelection() {
        // Default Logo
        mView.setDefaultGoogleLogoDrawable(
                mView.getContext().getDrawable(R.drawable.ic_google_logo));
        mView.maybeShowDefaultLogoDrawable();
        mView.endAnimationsForTesting();
        Assert.assertEquals(ImageView.ScaleType.CENTER_INSIDE, mView.getScaleType());

        // Doodle
        Logo logo =
                new Logo(
                        /* image= */ mBitmap,
                        /* darkImage= */ mDarkBitmap,
                        /* onClickUrl= */ null,
                        /* altText= */ ALT_TEXT,
                        /* animatedLogoUrl= */ null,
                        /* darkAnimatedLogoUrl= */ null,
                        /* logUrl= */ null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        Assert.assertEquals(ImageView.ScaleType.FIT_CENTER, mView.getScaleType());
    }

    @Test
    public void testLogoView_DarkMode_WithDarkAsset() {
        Bitmap darkBitmap = Bitmap.createBitmap(2, 2, Config.ARGB_8888);
        Logo logo =
                new Logo(
                        mBitmap,
                        darkBitmap,
                        null,
                        null,
                        ANIMATED_LOGO_URL,
                        DARK_ANIMATED_LOGO_URL,
                        /* logUrl= */ null);

        // Test Light Mode
        mView.setNightMode(false);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        Assert.assertEquals(
                "Should render light logo in light mode",
                mBitmap,
                getLogoDrawableBitmapForTesting());
        Assert.assertEquals(
                "Should use light animated logo in light mode",
                ANIMATED_LOGO_URL,
                mView.getAnimatedLogoUrlForTesting());
        Assert.assertTrue("Logo should be clickable", mView.isClickable());

        // Test Dark Mode
        mView.setNightMode(true);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        Assert.assertEquals(
                "Should render dark logo in dark mode",
                darkBitmap,
                getLogoDrawableBitmapForTesting());
        Assert.assertEquals(
                "Should use dark animated logo in dark mode",
                DARK_ANIMATED_LOGO_URL,
                mView.getAnimatedLogoUrlForTesting());
        Assert.assertTrue("Logo should be clickable", mView.isClickable());
    }

    @Test
    public void testLogoView_DarkMode_WithoutDarkAsset() {
        Logo logo =
                new Logo(mBitmap, null, null, null, ANIMATED_LOGO_URL, null, /* logUrl= */ null);

        // Test Dark Mode Fallback
        mView.setNightMode(true);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        Assert.assertEquals(
                "Should fall back to light logo in dark mode if no dark asset exists",
                mBitmap,
                getLogoDrawableBitmapForTesting());
        Assert.assertEquals(
                "Should fall back to light animated logo in dark mode if no dark animated logo"
                        + " exists",
                ANIMATED_LOGO_URL,
                mView.getAnimatedLogoUrlForTesting());
        Assert.assertTrue("Logo should be clickable", mView.isClickable());
    }

    private Bitmap getLogoDrawableBitmapForTesting() {
        Drawable drawable = mView.getLogoDrawableForTesting();
        if (drawable instanceof BitmapDrawable) {
            return ((BitmapDrawable) drawable).getBitmap();
        }
        return null;
    }
}
