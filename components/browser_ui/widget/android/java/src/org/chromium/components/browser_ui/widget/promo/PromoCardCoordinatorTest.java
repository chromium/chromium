// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.test.InstrumentationRegistry;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Basic test for creating, using the promo component with {@link PromoCardCoordinator}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PromoCardCoordinatorTest {
    private Context mContext;
    private PropertyModel mModel;
    private PromoCardCoordinator mPromoCardCoordinator;
    private PromoCardView mView;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mModel = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> new PropertyModel.Builder(PromoCardProperties.ALL_KEYS).build());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(mPromoCardCoordinator::destroy);
    }

    private void setupCoordinator(@LayoutStyle int layoutStyle) {
        // TODO(https://crbug.com/1217140): Remove this theme wrapper after dummy ui activity
        //  is based on material theme. For now we need the theme wrapper to inflate the layout;
        //  because we are not setting our theme overlay for the test apk
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ContextThemeWrapper wrapperTheme =
                    new ContextThemeWrapper(mContext, R.style.Theme_BrowserUI_DayNight);
            mPromoCardCoordinator =
                    new PromoCardCoordinator(wrapperTheme, mModel, "test-feature", layoutStyle);
            mView = (PromoCardView) mPromoCardCoordinator.getView();
        });
        Assert.assertNotNull("PromoCardView is null", mView);
    }

    @Test
    @SmallTest
    public void testCreateView_Large() {
        setupCoordinator(LayoutStyle.LARGE);

        Assert.assertNotNull("Large promo should have image view.", mView.mPromoImage);
        Assert.assertNotNull("Large promo should have title.", mView.mTitle);
        Assert.assertNotNull("Large promo should have description.", mView.mDescription);
        Assert.assertNotNull("Large promo should have primary button.", mView.mPrimaryButton);
        Assert.assertNotNull("Large promo should have secondary button.", mView.mSecondaryButton);
    }

    @Test
    @SmallTest
    public void testCreateView_Compact() {
        setupCoordinator(LayoutStyle.COMPACT);

        Assert.assertNotNull("Compact promo should have image view.", mView.mPromoImage);
        Assert.assertNotNull("Compact promo should have title.", mView.mTitle);
        Assert.assertNotNull("Compact promo should have description.", mView.mDescription);
        Assert.assertNotNull("Compact promo should have primary button.", mView.mPrimaryButton);
        Assert.assertNotNull("Compact promo should have secondary button.", mView.mSecondaryButton);
    }

    @Test
    @SmallTest
    public void testCreateView_Slim() {
        setupCoordinator(LayoutStyle.SLIM);

        Assert.assertNotNull("Slim Promo should have image view.", mView.mPromoImage);
        Assert.assertNotNull("Slim Promo should have title.", mView.mTitle);
        Assert.assertNotNull("Slim Promo should have primary button.", mView.mPrimaryButton);

        Assert.assertNull("Slim promo should not have description.", mView.mDescription);
        Assert.assertNull("Slim promo should not have secondary button.", mView.mSecondaryButton);
    }

    @Test
    @SmallTest
    public void testTextImageBinding() {
        setupCoordinator(LayoutStyle.LARGE);
        final Drawable testImage =
                AppCompatResources.getDrawable(mContext, R.drawable.test_logo_avatar_anonymous);
        final String titleString = "Some string for title";
        final String testString = "Some test string";
        final String primaryButtonString = "Primary button string";
        final String secondaryButtonString = "Secondary button string";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(PromoCardProperties.TITLE, titleString);
            mModel.set(PromoCardProperties.DESCRIPTION, testString);
            mModel.set(PromoCardProperties.IMAGE, testImage);
            mModel.set(PromoCardProperties.PRIMARY_BUTTON_TEXT, primaryButtonString);
            mModel.set(PromoCardProperties.SECONDARY_BUTTON_TEXT, secondaryButtonString);
        });

        Assert.assertEquals(
                "Promo image drawable is different.", testImage, mView.mPromoImage.getDrawable());
        Assert.assertEquals(
                "Promo title is different.", titleString, mView.mTitle.getText().toString());
        Assert.assertEquals("Promo description is different.", testString,
                mView.mDescription.getText().toString());
        Assert.assertEquals("Promo primary button text is different.", primaryButtonString,
                mView.mPrimaryButton.getText().toString());
        Assert.assertEquals("Promo secondary button text is different.", secondaryButtonString,
                mView.mSecondaryButton.getText().toString());

        // Change the description again
        final String testString2 = "Some other test string.";
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.set(PromoCardProperties.DESCRIPTION, testString2); });
        Assert.assertEquals(testString2, mView.mDescription.getText().toString());
    }

    @Test
    @SmallTest
    public void testChangeVisibility() {
        setupCoordinator(LayoutStyle.LARGE);
        Assert.assertEquals(mView.mSecondaryButton.getVisibility(), View.VISIBLE);

        // Hide the secondary button
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.set(PromoCardProperties.HAS_SECONDARY_BUTTON, false); });
        Assert.assertEquals("Secondary button is still visible.", View.GONE,
                mView.mSecondaryButton.getVisibility());
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/1232932")
    public void testActionBinding() throws Exception {
        setupCoordinator(LayoutStyle.LARGE);
        final CallbackHelper primaryClickCallback = new CallbackHelper();
        final CallbackHelper secondaryClickCallback = new CallbackHelper();

        mModel.set(PromoCardProperties.PRIMARY_BUTTON_CALLBACK,
                (v) -> primaryClickCallback.notifyCalled());
        mModel.set(PromoCardProperties.SECONDARY_BUTTON_CALLBACK,
                (v) -> secondaryClickCallback.notifyCalled());

        TestThreadUtils.runOnUiThreadBlocking(() -> mView.mPrimaryButton.performClick());
        primaryClickCallback.waitForCallback("Primary button callback is never called.", 0);
        Assert.assertEquals(
                "Primary button should be clicked once.", 1, primaryClickCallback.getCallCount());

        TestThreadUtils.runOnUiThreadBlocking(() -> mView.mSecondaryButton.performClick());
        secondaryClickCallback.waitForCallback("Secondary button callback is never called.", 0);
        Assert.assertEquals("Secondary button should be clicked once.", 1,
                secondaryClickCallback.getCallCount());
    }
}
