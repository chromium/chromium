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
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
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
        MockitoAnnotations.initMocks(this);
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
    public void testDefaultLogoView() {
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
    public void testShowLoadingView() {
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
    @MediumTest
    @EnableFeatures({ChromeFeatureList.LOGO_POLISH})
    public void testDoodleAnimation() {
        mView.setIsLogoPolishFlagEnabledForTesting(true);
        Resources res = mView.getResources();
        int normalLogoHeight = res.getDimensionPixelSize(R.dimen.ntp_logo_height);
        int normalLogoTopMargin = res.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
        int logoHeightForLogoPolish = LogoUtils.getLogoHeightForLogoPolishWithSmallSize(res);
        int logoTopMarginForLogoPolish = LogoUtils.getTopMarginForLogoPolish(res);

        MarginLayoutParams logoLayoutParams = (MarginLayoutParams) mView.getLayoutParams();

        // Test default google logo.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogo(
                new CachedTintedBitmap(R.drawable.google_logo, R.color.google_logo_tint_color)
                        .getBitmap(mView.getContext()));
        mView.updateLogo(null);
        mView.endAnimationsForTesting();
        Assert.assertEquals(normalLogoHeight, logoLayoutParams.height);
        Assert.assertEquals(normalLogoTopMargin, logoLayoutParams.topMargin);

        // Test doodle animation.
        Logo logo = new Logo(mBitmap, null, ALT_TEXT, null);
        mModel.set(LogoProperties.LOGO, logo);
        ObjectAnimator fadeAnimation = mView.getFadeAnimationForTesting();
        Assert.assertNotNull(fadeAnimation);

        fadeAnimation.pause();

        fadeAnimation.setCurrentFraction(0);
        Assert.assertEquals(normalLogoHeight, logoLayoutParams.height);
        Assert.assertEquals(normalLogoTopMargin, logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.3F);
        Assert.assertEquals(normalLogoHeight, logoLayoutParams.height);
        Assert.assertEquals(normalLogoTopMargin, logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.5F);
        Assert.assertEquals(normalLogoHeight, logoLayoutParams.height);
        Assert.assertEquals(normalLogoTopMargin, logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.65F);
        Assert.assertEquals(
                Math.round((normalLogoHeight + (logoHeightForLogoPolish - normalLogoHeight) * 0.3)),
                logoLayoutParams.height);
        Assert.assertEquals(
                Math.round(
                        (normalLogoTopMargin
                                + (logoTopMarginForLogoPolish - normalLogoTopMargin) * 0.3)),
                logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(0.75F);
        Assert.assertEquals(
                Math.round((normalLogoHeight + logoHeightForLogoPolish) * 0.5),
                logoLayoutParams.height);
        Assert.assertEquals(
                Math.round((normalLogoTopMargin + logoTopMarginForLogoPolish) * 0.5),
                logoLayoutParams.topMargin);

        fadeAnimation.setCurrentFraction(1);
        Assert.assertEquals(logoHeightForLogoPolish, logoLayoutParams.height);
        Assert.assertEquals(logoTopMarginForLogoPolish, logoLayoutParams.topMargin);
    }
}
