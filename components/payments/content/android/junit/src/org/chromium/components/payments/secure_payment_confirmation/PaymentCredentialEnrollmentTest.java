// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** A test for PaymentCredentialEnrollment. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {PaymentCredentialEnrollmentTest.ShadowBottomSheetControllerProvider.class})
public class PaymentCredentialEnrollmentTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;

    private boolean mIsEnrollmentConfirmed;
    private boolean mIsEnrollmentCancelled;
    private Callback<Boolean> mCallback;

    private Bitmap mIcon;
    private PaymentCredentialEnrollmentController mEnrollmentController;

    /**
     * The shadow of BottomSheetControllerProvider. Not to use outside the test.
     */
    @Implements(BottomSheetControllerProvider.class)
    /* package */ static class ShadowBottomSheetControllerProvider {
        private static BottomSheetController sBottomSheetController;

        @Implementation
        public static BottomSheetController from(WindowAndroid windowAndroid) {
            return sBottomSheetController;
        }

        private static void setBottomSheetController(BottomSheetController controller) {
            sBottomSheetController = controller;
        }
    }

    @Before
    public void setUp() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        setWindowAndroid(windowAndroid, mWebContents);
        Mockito.doReturn(new WeakReference<Context>(RuntimeEnvironment.application))
                .when(windowAndroid)
                .getContext();

        mCallback = (response) -> {
            if (response) {
                mIsEnrollmentConfirmed = true;
            } else {
                mIsEnrollmentCancelled = true;
            }
        };

        mIcon = Mockito.mock(Bitmap.class);
        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/*requestShowContentResponse=*/true));
    }

    @After
    public void tearDown() {
        if (mEnrollmentController != null) mEnrollmentController.hide();
    }

    private void createEnrollmentController() {
        mEnrollmentController = PaymentCredentialEnrollmentController.create(mWebContents);
    }

    private BottomSheetController createBottomSheetController(boolean requestShowContentResponse) {
        BottomSheetController controller = Mockito.mock(BottomSheetController.class);
        Mockito.doReturn(requestShowContentResponse)
                .when(controller)
                .requestShowContent(Mockito.any(BottomSheetContent.class), Mockito.anyBoolean());
        return controller;
    }

    private boolean show() {
        if (mEnrollmentController == null) return false;

        mIsEnrollmentConfirmed = false;
        mIsEnrollmentCancelled = false;
        return mEnrollmentController.show(mCallback, "paymentInstrumentLabel", mIcon, false);
    }

    private void setWindowAndroid(WindowAndroid windowAndroid, WebContents webContents) {
        Mockito.doReturn(windowAndroid).when(webContents).getTopLevelNativeWindow();
    }

    private void setContext(Context context) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        Mockito.doReturn(new WeakReference<>(context)).when(windowAndroid).getContext();
    }

    @Test
    @Feature({"Payments"})
    public void testOnEnrollmentConfirmation() {
        createEnrollmentController();
        show();
        mEnrollmentController.getView().mContinueButton.performClick();
        Assert.assertTrue(mIsEnrollmentConfirmed);
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testOnEnrollmentButtonCancellation() {
        createEnrollmentController();
        show();
        mEnrollmentController.getView().mCancelButton.performClick();
        Assert.assertTrue(mIsEnrollmentCancelled);
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testHide() {
        createEnrollmentController();
        show();
        mEnrollmentController.hide();
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testRequestShowContentFalse() {
        createEnrollmentController();
        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/*requestShowContentResponse=*/false));
        Assert.assertFalse(show());
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullWindowAndroid() {
        setWindowAndroid(null, mWebContents);
        createEnrollmentController();
        Assert.assertFalse(show());
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullContext() {
        setContext(null);
        createEnrollmentController();
        Assert.assertFalse(show());
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullBottomSheetController() {
        ShadowBottomSheetControllerProvider.setBottomSheetController(null);
        createEnrollmentController();
        Assert.assertFalse(show());
        Assert.assertTrue(mEnrollmentController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testCreateWithNullWebContents() {
        mWebContents = null;
        createEnrollmentController();
        Assert.assertNull(mEnrollmentController);
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithHide() {
        createEnrollmentController();
        Assert.assertTrue(show());
        mEnrollmentController.hide();
        Assert.assertTrue(show());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithoutHide() {
        createEnrollmentController();
        Assert.assertTrue(show());
        Assert.assertFalse(show());
    }
}
