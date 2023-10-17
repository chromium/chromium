// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.content.Context;
import android.view.View;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.test_support.FakeClock;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** A test for SecurePaymentConfirmationNoMatchingCred. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            SecurePaymentConfirmationNoMatchingCredTest.ShadowBottomSheetControllerProvider.class
        })
public class SecurePaymentConfirmationNoMatchingCredTest {
    private static final long IGNORED_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100;
    private static final long SAFE_INPUT_DELAY =
            InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private WebContents mWebContents;

    private boolean mUserResponded;
    private boolean mUserOptedOut;
    private Runnable mResponseCallback;
    private Runnable mOptOutCallback;
    private FakeClock mClock;

    private SecurePaymentConfirmationNoMatchingCredController mNoMatchingCredController;

    /** The shadow of BottomSheetControllerProvider. Not to use outside the test. */
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

        // Create formatter mocks
        UrlFormatter.Natives urlFormatterJniMock = Mockito.mock(UrlFormatter.Natives.class);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, urlFormatterJniMock);
        Mockito.doReturn("example.test")
                .when(urlFormatterJniMock)
                .formatStringUrlForSecurityDisplay(
                        Mockito.any(), Mockito.eq(SchemeDisplay.OMIT_CRYPTOGRAPHIC));

        mResponseCallback =
                () -> {
                    mUserResponded = true;
                };
        mOptOutCallback =
                () -> {
                    mUserOptedOut = true;
                };

        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/* requestShowContentResponse= */ true));

        mClock = new FakeClock();
    }

    @After
    public void tearDown() {
        if (mNoMatchingCredController != null) mNoMatchingCredController.close();
    }

    private void createNoMatchingCredController() {
        mNoMatchingCredController =
                SecurePaymentConfirmationNoMatchingCredController.create(mWebContents);
        // Some tests expect a null controller, e.g. for a null web contents.
        if (mNoMatchingCredController != null) {
            mNoMatchingCredController.setInputProtectorForTesting(new InputProtector(mClock));
        }
    }

    private BottomSheetController createBottomSheetController(boolean requestShowContentResponse) {
        BottomSheetController controller = Mockito.mock(BottomSheetController.class);
        Mockito.doReturn(requestShowContentResponse)
                .when(controller)
                .requestShowContent(Mockito.any(BottomSheetContent.class), Mockito.anyBoolean());
        return controller;
    }

    private boolean show() {
        return show(/* enableOptOut= */ false);
    }

    private boolean show(boolean enableOptOut) {
        if (mNoMatchingCredController == null) return false;

        mUserResponded = false;
        mUserOptedOut = false;

        String rpId = "rp.example";
        return mNoMatchingCredController.show(
                mResponseCallback, mOptOutCallback, enableOptOut, rpId);
    }

    private void setWindowAndroid(WindowAndroid windowAndroid, WebContents webContents) {
        Mockito.doReturn(windowAndroid).when(webContents).getTopLevelNativeWindow();
    }

    private void setContext(Context context) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        Mockito.doReturn(new WeakReference<Context>(context)).when(windowAndroid).getContext();
    }

    @Test
    @Feature({"Payments"})
    public void testShow() {
        createNoMatchingCredController();
        show();
        SecurePaymentConfirmationNoMatchingCredView view = mNoMatchingCredController.getView();
        Assert.assertNotNull(view);
        Assert.assertTrue(view.mDescription.getText().toString().contains("example.test"));
        // Opt-out text should be hidden by default.
        Assert.assertEquals(View.GONE, view.mOptOutText.getVisibility());
    }

    @Test
    @Feature({"Payments"})
    public void testShowRendersOptOutWhenRequested() {
        createNoMatchingCredController();
        show(/* enableOptOut= */ true);
        SecurePaymentConfirmationNoMatchingCredView view = mNoMatchingCredController.getView();
        Assert.assertNotNull(view);
        Assert.assertEquals(View.VISIBLE, view.mOptOutText.getVisibility());
        Assert.assertTrue(view.mOptOutText.getText().toString().contains("rp.example"));
    }

    @Test
    @Feature({"Payments"})
    public void testShowOnResponse() {
        createNoMatchingCredController();
        show();
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        mNoMatchingCredController.getView().mOkButton.performClick();
        Assert.assertTrue(mUserResponded);
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowOnOptOut() {
        createNoMatchingCredController();
        show(/* enableOptOut= */ true);
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        SecurePaymentConfirmationNoMatchingCredView credView = mNoMatchingCredController.getView();
        credView.mOptOutText.getClickableSpans()[0].onClick(credView.mOptOutText);
        Assert.assertTrue(mUserOptedOut);
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testUnintentedInput() {
        createNoMatchingCredController();
        show(/* enableOptOut= */ true);

        // Clicking immediately is prevented.
        mNoMatchingCredController.getView().mOkButton.performClick();
        Assert.assertFalse(mUserResponded);
        Assert.assertFalse(mNoMatchingCredController.isHidden());

        SecurePaymentConfirmationNoMatchingCredView credView = mNoMatchingCredController.getView();
        credView.mOptOutText.getClickableSpans()[0].onClick(credView.mOptOutText);
        Assert.assertFalse(mUserOptedOut);
        Assert.assertFalse(mNoMatchingCredController.isHidden());

        // Clicking after an interval less than the threshold is still prevented.
        mClock.advanceCurrentTimeMillis(IGNORED_INPUT_DELAY);

        mNoMatchingCredController.getView().mOkButton.performClick();
        Assert.assertFalse(mUserResponded);
        Assert.assertFalse(mNoMatchingCredController.isHidden());

        credView.mOptOutText.getClickableSpans()[0].onClick(credView.mOptOutText);
        Assert.assertFalse(mUserOptedOut);
        Assert.assertFalse(mNoMatchingCredController.isHidden());

        // Clicking confirm after the threshold is no longer prevented and closes the dialog.
        mClock.advanceCurrentTimeMillis(SAFE_INPUT_DELAY);
        mNoMatchingCredController.getView().mOkButton.performClick();
        Assert.assertTrue(mUserResponded);
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testRequestShowContentFalse() {
        createNoMatchingCredController();
        ShadowBottomSheetControllerProvider.setBottomSheetController(
                createBottomSheetController(/* requestShowContentResponse= */ false));
        Assert.assertFalse(show());
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testCreateWithNullWebContents() {
        mWebContents = null;
        createNoMatchingCredController();
        Assert.assertNull(mNoMatchingCredController);
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullWindowAndroid() {
        setWindowAndroid(null, mWebContents);
        createNoMatchingCredController();
        Assert.assertFalse(show());
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullContext() {
        setContext(null);
        createNoMatchingCredController();
        Assert.assertFalse(show());
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowWithNullBottomSheetController() {
        ShadowBottomSheetControllerProvider.setBottomSheetController(null);
        createNoMatchingCredController();
        Assert.assertFalse(show());
        Assert.assertTrue(mNoMatchingCredController.isHidden());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithClose() {
        createNoMatchingCredController();
        Assert.assertTrue(show());
        mNoMatchingCredController.close();
        Assert.assertTrue(show());
    }

    @Test
    @Feature({"Payments"})
    public void testShowTwiceWithoutClose() {
        createNoMatchingCredController();
        Assert.assertTrue(show());
        Assert.assertFalse(show());
    }
}
