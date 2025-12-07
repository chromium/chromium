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
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;

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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
    private static final String ALT_TEXT = "Hello World!";

    private LogoView mView;
    private Bitmap mBitmap;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private PropertyModel mModel;

    @Before
    public void setup() {
        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mView = new LogoView(activity, null);
                            LayoutParams params =
                                    new LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                            activity.setContentView(mView, params);
                            mModel = new PropertyModel(LogoProperties.ALL_KEYS);
                            mPropertyModelChangeProcessor =
                                    PropertyModelChangeProcessor.create(
                                            mModel, mView, new LogoViewBinder());
                        });
    }

    @Test
    public void testDefaultLogoView_refactorDisabled() {
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogo(
                new CachedTintedBitmap(R.drawable.google_logo, R.color.google_logo_tint_color)
                        .getBitmap(mView.getContext()));
        mView.updateLogo(null);
        mView.endAnimationsForTesting();

        Assert.assertFalse("Default logo should not be clickable.", mView.isClickable());
        Assert.assertFalse("Default logo should not be focusable.", mView.isFocusable());
        Assert.assertTrue(
                "Default logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testDefaultLogoView() {
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogoDrawable(
                mView.getContext().getDrawable(R.drawable.ic_google_logo));
        mView.updateLogo(null);
        mView.endAnimationsForTesting();

        Assert.assertFalse("Default logo should not be clickable.", mView.isClickable());
        Assert.assertFalse("Default logo should not be focusable.", mView.isFocusable());
        Assert.assertTrue(
                "Default logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithUrl() {
        Logo logo = new Logo(mBitmap, LOGO_URL, null, null);
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
        Logo logo = new Logo(mBitmap, null, null, ANIMATED_LOGO_URL);
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
        Logo logo = new Logo(mBitmap, LOGO_URL, null, null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        mView.performClick();
        verify(mLogoClickHandler, times(1)).onLogoClicked(false);
    }

    @Test
    public void testLogoView_WithAltText() {
        Logo logo = new Logo(mBitmap, null, ALT_TEXT, null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertFalse("Logo without URL should not be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with alt text should be focusable.", mView.isFocusable());
        Assert.assertFalse(
                "Logo should have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_LOGO_VIEW_REFACTOR})
    public void testShowLoadingView_disabled() {
        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8), null, null, null);
        mModel.set(LogoProperties.LOGO, logo);
        mView.endAnimationsForTesting();
        Assert.assertNotNull(mView.getLogoForTesting());
        mView.setLoadingViewVisibilityForTesting(View.VISIBLE);
        mModel.set(LogoProperties.SHOW_LOADING_VIEW, true);
        Assert.assertNull(mView.getLogoForTesting());
        Assert.assertEquals(View.GONE, mView.getLoadingViewVisibilityForTesting());
    }

    @Test
    public void testShowLoadingView() {
        Logo logo = new Logo(Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8), null, null, null);
        mModel.set(LogoProperties.LOGO, logo);
        mView.endAnimationsForTesting();
        Assert.assertNotNull(mView.getLogoDrawableForTesting());
        mView.setLoadingViewVisibilityForTesting(View.VISIBLE);
        mModel.set(LogoProperties.SHOW_LOADING_VIEW, true);
        Assert.assertNull(mView.getLogoDrawableForTesting());
        Assert.assertEquals(View.GONE, mView.getLoadingViewVisibilityForTesting());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_LOGO_VIEW_REFACTOR})
    public void testDoodleAnimation_disabled() {
        // Test default google logo.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogo(
                new CachedTintedBitmap(R.drawable.google_logo, R.color.google_logo_tint_color)
                        .getBitmap(mView.getContext()));

        testDoodleAnimationImpl();
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

        mView.updateLogo(null);
        mView.endAnimationsForTesting();
        Assert.assertEquals(logoHeight, logoLayoutParams.height);
        Assert.assertEquals(logoTopMargin, logoLayoutParams.topMargin);

        // Test doodle animation.
        Logo logo = new Logo(mBitmap, null, ALT_TEXT, null);
        mModel.set(LogoProperties.LOGO, logo);
        ObjectAnimator fadeAnimation = mView.getFadeAnimationForTesting();
        Assert.assertNotNull(fadeAnimation);

        fadeAnimation.pause();

        fadeAnimation.setCurrentFraction(0);
        Assert.assertEquals(logoHeight, logoLayoutParams.height);
        Assert.assertEquals(logoTopMargin, logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.3F);
        Assert.assertEquals(logoHeight, logoLayoutParams.height);
        Assert.assertEquals(logoTopMargin, logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.5F);
        Assert.assertEquals(logoHeight, logoLayoutParams.height);
        Assert.assertEquals(logoTopMargin, logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.65F);
        Assert.assertEquals(
                Math.round((logoHeight + (doodleHeight - logoHeight) * 0.3)),
                logoLayoutParams.height);
        Assert.assertEquals(
                Math.round((logoTopMargin + (doodleTopMargin - logoTopMargin) * 0.3)),
                logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.75F);
        Assert.assertEquals(Math.round((logoHeight + doodleHeight) * 0.5), logoLayoutParams.height);
        Assert.assertEquals(
                Math.round((logoTopMargin + doodleTopMargin) * 0.5), logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(1);
        Assert.assertEquals(doodleHeight, logoLayoutParams.height);
        Assert.assertEquals(doodleTopMargin, logoLayoutParams.topMargin);
    }
}
