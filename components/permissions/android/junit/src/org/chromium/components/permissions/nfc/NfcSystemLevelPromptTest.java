// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions.nfc;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.content.Intent;
import android.provider.Settings;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.permissions.test.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Tests for the {@link NfcSystemLevelPrompt} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class NfcSystemLevelPromptTest {
    private NfcSystemLevelPrompt mNfcSystemLevelPrompt;
    private Activity mActivity;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WindowAndroid.IntentCallback mWindowAndroidIntentCallback;
    private CallbackHelper mDialogCallback = new CallbackHelper();
    private CallbackHelper mIntentCallback = new CallbackHelper();
    private MockModalDialogManager mModalDialogManager = new MockModalDialogManager();

    private static class MockModalDialogManager extends ModalDialogManager {
        private PropertyModel mShownDialogModel;

        public MockModalDialogManager() {
            super(Mockito.mock(Presenter.class), 0);
        }

        @Override
        public void showDialog(PropertyModel model, int dialogType) {
            mShownDialogModel = model;
        }

        @Override
        public void dismissDialog(PropertyModel model, int dismissalCause) {
            model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
        }

        public PropertyModel getShownDialogModel() {
            return mShownDialogModel;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        doReturn(new WeakReference<>(mActivity)).when(mWindowAndroid).getActivity();

        doAnswer(
                        invocation -> {
                            Object intent = invocation.getArguments()[0];
                            String intentAction = ((Intent) intent).getAction();
                            Assert.assertEquals(intentAction, Settings.ACTION_NFC_SETTINGS);

                            Object intentCallback = invocation.getArguments()[1];
                            mWindowAndroidIntentCallback =
                                    (WindowAndroid.IntentCallback) intentCallback;

                            mIntentCallback.notifyCalled();
                            return null;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(WindowAndroid.IntentCallback.class), isNull());

        doAnswer(
                        invocation -> {
                            mDialogCallback.notifyCalled();
                            return null;
                        })
                .when(mWindowAndroidIntentCallback)
                .onIntentCompleted(anyInt(), any(Intent.class));

        mNfcSystemLevelPrompt = new NfcSystemLevelPrompt();
        mNfcSystemLevelPrompt.show(
                mWindowAndroid,
                mModalDialogManager,
                new Runnable() {
                    @Override
                    public void run() {
                        mDialogCallback.notifyCalled();
                    }
                });
    }

    /** Tests whether callback for dismissal functions correctly. */
    @Test
    public void testDismissCallback() {
        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(shownDialogModel);
        Assert.assertEquals(0, mDialogCallback.getCallCount());
        Assert.assertEquals(0, mIntentCallback.getCallCount());

        shownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(shownDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Assert.assertEquals(1, mDialogCallback.getCallCount());
        Assert.assertEquals(0, mIntentCallback.getCallCount());
    }

    /** Tests whether intent and callback for clicking on the 'Turn on' button functions correctly. */
    @Test
    public void testTurnOnCallback() {
        PropertyModel shownDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(shownDialogModel);
        Assert.assertEquals(0, mDialogCallback.getCallCount());
        Assert.assertEquals(0, mIntentCallback.getCallCount());

        shownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(shownDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Assert.assertEquals(0, mDialogCallback.getCallCount());
        Assert.assertEquals(1, mIntentCallback.getCallCount());

        mWindowAndroidIntentCallback.onIntentCompleted(/* resultCode= */ 0, new Intent());
        Assert.assertEquals(1, mDialogCallback.getCallCount());
        Assert.assertEquals(1, mIntentCallback.getCallCount());
    }
}
