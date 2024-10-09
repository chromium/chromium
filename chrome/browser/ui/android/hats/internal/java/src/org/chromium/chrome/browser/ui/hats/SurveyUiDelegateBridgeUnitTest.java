// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageWrapper;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link SurveyUiDelegateBridge} on Java */
@RunWith(BaseRobolectricTestRunner.class)
public class SurveyUiDelegateBridgeUnitTest {
    private static final long TEST_NATIVE_POINTER = 123541L;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public TestSurveyUtils.TestSurveyComponentRule mSurveyTestRule =
            new TestSurveyUtils.TestSurveyComponentRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SurveyUiDelegateBridge.Natives mMockSurveyUiDelegateBridge;
    @Mock private ManagedMessageDispatcher mMockMessageDispatcher;
    @Mock private TabModelSelector mTabModelSelector;

    private Activity mActivity;
    private WindowAndroid mWindow;

    @Before
    public void setup() {
        mJniMocker.mock(SurveyUiDelegateBridgeJni.TEST_HOOKS, mMockSurveyUiDelegateBridge);

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mWindow =
                new ActivityWindowAndroid(
                        mActivity, false, IntentRequestTracker.createFromActivity(mActivity));
        MessagesFactory.attachMessageDispatcher(mWindow, mMockMessageDispatcher);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
    }

    @After
    public void tearDown() {
        MessagesFactory.detachMessageDispatcher(mMockMessageDispatcher);
        mActivity.finish();
        mWindow.destroy();
    }

    @Test
    public void createNativeDelegateBridge() {
        SurveyUiDelegate delegate = SurveyUiDelegateBridge.create(TEST_NATIVE_POINTER);
        assertNotNull(delegate);

        delegate.showSurveyInvitation(
                CallbackUtils.emptyRunnable(),
                CallbackUtils.emptyRunnable(),
                CallbackUtils.emptyRunnable());
        verify(mMockSurveyUiDelegateBridge)
                .showSurveyInvitation(eq(TEST_NATIVE_POINTER), notNull(), notNull(), notNull());

        delegate.dismiss();
        verify(mMockSurveyUiDelegateBridge).dismiss(eq(TEST_NATIVE_POINTER));
    }

    @Test
    @Config(shadows = ShadowMessageSurveyUiDelegate.class)
    public void createBridgeFromMessage_Success() {
        MessageWrapper wrapper = MessageWrapper.create(1L, /*MessageIdentifier.INVALID_MESSAGE*/ 0);
        SurveyUiDelegateBridge delegate =
                SurveyUiDelegateBridge.createFromMessage(TEST_NATIVE_POINTER, wrapper, mWindow);
        assertNotNull(delegate);

        ShadowMessageSurveyUiDelegate testDelegate =
                Shadow.extract(delegate.getDelegateForTesting());

        delegate.showSurveyInvitation(() -> {}, () -> {}, () -> {});
        verify(testDelegate.mMockDelegate).showSurveyInvitation(notNull(), notNull(), notNull());

        delegate.dismiss();
        verify(testDelegate.mMockDelegate).dismiss();

        verifyNoInteractions(mMockSurveyUiDelegateBridge);
    }

    @Test
    public void createFromMessage_FailedWithoutWindow() {
        MessageWrapper wrapper = MessageWrapper.create(1L, /*MessageIdentifier.INVALID_MESSAGE*/ 0);
        SurveyUiDelegateBridge delegate =
                SurveyUiDelegateBridge.createFromMessage(TEST_NATIVE_POINTER, wrapper, null);
        assertNull(delegate);
    }

    @Test
    public void createBridgeFromMessage_FailedWithoutTabModel() {
        TabModelSelectorSupplier.setInstanceForTesting(null);
        MessageWrapper wrapper = MessageWrapper.create(1L, /*MessageIdentifier.INVALID_MESSAGE*/ 0);
        SurveyUiDelegateBridge delegate =
                SurveyUiDelegateBridge.createFromMessage(TEST_NATIVE_POINTER, wrapper, mWindow);
        assertNull(delegate);
    }

    @Test
    public void createBridgeFromMessage_FailedWithoutMessageDispatcher() {
        MessagesFactory.detachMessageDispatcher(mMockMessageDispatcher);
        MessageWrapper wrapper = MessageWrapper.create(1L, /*MessageIdentifier.INVALID_MESSAGE*/ 0);
        SurveyUiDelegateBridge delegate =
                SurveyUiDelegateBridge.createFromMessage(TEST_NATIVE_POINTER, wrapper, mWindow);
        assertNull(delegate);
    }

    @Implements(MessageSurveyUiDelegate.class)
    public static class ShadowMessageSurveyUiDelegate {
        private final SurveyUiDelegate mMockDelegate;

        public ShadowMessageSurveyUiDelegate() {
            mMockDelegate = Mockito.mock(SurveyUiDelegate.class);
        }

        @Implementation
        protected void showSurveyInvitation(
                Runnable onSurveyAccepted,
                Runnable onSurveyDeclined,
                Runnable onSurveyPresentationFailed) {
            mMockDelegate.showSurveyInvitation(
                    onSurveyAccepted, onSurveyDeclined, onSurveyPresentationFailed);
        }

        @Implementation
        protected void dismiss() {
            mMockDelegate.dismiss();
        }
    }
}
