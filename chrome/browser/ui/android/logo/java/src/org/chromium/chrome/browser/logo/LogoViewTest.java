// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.text.TextUtils;
import android.view.ViewGroup.LayoutParams;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.TestActivity;

/** Instrumentation tests for {@link LogoView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LogoViewTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    public TemplateUrlService mTemplateUrlService;
    @Mock
    public LogoDelegateImpl mLogoDelegate;

    private static final String LOGO_URL = "https://www.google.com";
    private static final String ANIMATED_LOGO_URL =
            "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json";
    private static final String ALT_TEXT = "Hello World!";

    private LogoView mView;
    private Bitmap mBitmap;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        mActivityScenarioRule.getScenario().onActivity(activity -> {
            mView = new LogoView(activity, null);
            LayoutParams params =
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            activity.setContentView(mView, params);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TemplateUrlServiceFactory.setInstanceForTesting(null); });
    }

    @Test
    public void testDefaultLogoView() {
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mView.setDefaultGoogleLogo(LogoCoordinator.getDefaultGoogleLogo(mView.getContext()));
        mView.updateLogo(null);
        mView.endAnimationsForTesting();

        Assert.assertFalse("Default logo should not be clickable.", mView.isClickable());
        Assert.assertFalse("Default logo should not be focusable.", mView.isFocusable());
        Assert.assertTrue("Default logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithUrl() {
        Logo logo = new Logo(mBitmap, LOGO_URL, null, null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertTrue("Logo with URL should be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with URL should be focusable.", mView.isFocusable());
        Assert.assertTrue("Logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithAnimatedUrl() {
        Logo logo = new Logo(mBitmap, null, null, ANIMATED_LOGO_URL);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertTrue("Logo with animated URL should be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with animated URL should be focusable.", mView.isFocusable());
        Assert.assertTrue("Logo should not have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }

    @Test
    public void testLogoView_WithUrl_Clicked() {
        mView.setDelegate(mLogoDelegate);
        Logo logo = new Logo(mBitmap, LOGO_URL, null, null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();
        mView.performClick();
        verify(mLogoDelegate, times(1)).onLogoClicked(false);
    }

    @Test
    public void testLogoView_WithAltText() {
        Logo logo = new Logo(mBitmap, null, ALT_TEXT, null);
        mView.updateLogo(logo);
        mView.endAnimationsForTesting();

        Assert.assertFalse("Logo without URL should not be clickable.", mView.isClickable());
        Assert.assertTrue("Logo with alt text should be focusable.", mView.isFocusable());
        Assert.assertFalse("Logo should have a content description.",
                TextUtils.isEmpty(mView.getContentDescription()));
    }
}
