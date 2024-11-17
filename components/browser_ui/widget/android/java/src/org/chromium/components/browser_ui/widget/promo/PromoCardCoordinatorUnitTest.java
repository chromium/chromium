// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.ui.modelutil.PropertyModel;

/** Basic test for creating, using the promo component with {@link PromoCardCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PromoCardCoordinatorUnitTest {
    private Activity mActivity;
    private PropertyModel mModel;
    private PromoCardCoordinator mPromoCardCoordinator;
    private PromoCardView mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();
        mModel = new PropertyModel.Builder(PromoCardProperties.ALL_KEYS).build();
    }

    @After
    public void tearDown() {
        mPromoCardCoordinator.destroy();
    }

    private void setupCoordinator(@LayoutStyle int layoutStyle) {
        mPromoCardCoordinator =
                PromoCardCoordinator.create(mActivity, mModel, "test-feature", layoutStyle);
        mView = (PromoCardView) mPromoCardCoordinator.getView();
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
        final Drawable testImage = Mockito.mock(Drawable.class);
        final String titleString = "Some string for title";
        final String testString = "Some test string";
        final String primaryButtonString = "Primary button string";
        final String secondaryButtonString = "Secondary button string";

        mModel.set(PromoCardProperties.TITLE, titleString);
        mModel.set(PromoCardProperties.DESCRIPTION, testString);
        mModel.set(PromoCardProperties.IMAGE, testImage);
        mModel.set(PromoCardProperties.PRIMARY_BUTTON_TEXT, primaryButtonString);
        mModel.set(PromoCardProperties.SECONDARY_BUTTON_TEXT, secondaryButtonString);

        Assert.assertEquals(
                "Promo image drawable is different.", testImage, mView.mPromoImage.getDrawable());
        Assert.assertEquals(
                "Promo title is different.", titleString, mView.mTitle.getText().toString());
        Assert.assertEquals(
                "Promo description is different.",
                testString,
                mView.mDescription.getText().toString());
        Assert.assertEquals(
                "Promo primary button text is different.",
                primaryButtonString,
                mView.mPrimaryButton.getText().toString());
        Assert.assertEquals(
                "Promo secondary button text is different.",
                secondaryButtonString,
                mView.mSecondaryButton.getText().toString());

        // Change the description again
        final String testString2 = "Some other test string.";
        mModel.set(PromoCardProperties.DESCRIPTION, testString2);
        Assert.assertEquals(testString2, mView.mDescription.getText().toString());
    }

    @Test
    @SmallTest
    public void testChangeVisibility() {
        setupCoordinator(LayoutStyle.LARGE);
        Assert.assertEquals(mView.mSecondaryButton.getVisibility(), View.VISIBLE);

        // Hide the secondary button
        mModel.set(PromoCardProperties.HAS_SECONDARY_BUTTON, false);
        Assert.assertEquals(
                "Secondary button is still visible.",
                View.GONE,
                mView.mSecondaryButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testActionBinding() throws Exception {
        setupCoordinator(LayoutStyle.LARGE);
        final CallbackHelper primaryClickCallback = new CallbackHelper();
        final CallbackHelper secondaryClickCallback = new CallbackHelper();
        final CallbackHelper closeClickCallback = new CallbackHelper();

        mModel.set(
                PromoCardProperties.PRIMARY_BUTTON_CALLBACK,
                (v) -> primaryClickCallback.notifyCalled());
        mModel.set(
                PromoCardProperties.SECONDARY_BUTTON_CALLBACK,
                (v) -> secondaryClickCallback.notifyCalled());
        mModel.set(
                PromoCardProperties.CLOSE_BUTTON_CALLBACK,
                (v) -> closeClickCallback.notifyCalled());

        mView.mPrimaryButton.performClick();
        primaryClickCallback.waitForCallback("Primary button callback is never called.", 0);
        Assert.assertEquals(
                "Primary button should be clicked once.", 1, primaryClickCallback.getCallCount());

        mView.mSecondaryButton.performClick();
        secondaryClickCallback.waitForCallback("Secondary button callback is never called.", 0);
        Assert.assertEquals(
                "Secondary button should be clicked once.",
                1,
                secondaryClickCallback.getCallCount());

        mView.mCloseButton.performClick();
        closeClickCallback.waitForCallback("Secondary button callback is never called.", 0);
        Assert.assertEquals(
                "Close button should be clicked once.", 1, closeClickCallback.getCallCount());
    }
}
