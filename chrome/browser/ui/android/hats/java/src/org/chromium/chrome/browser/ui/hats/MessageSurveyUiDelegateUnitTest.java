// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.hats.MessageSurveyUiDelegate.State;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

/** Unit test for {@link MessageSurveyUiDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageSurveyUiDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private MessageSurveyUiDelegate mMessageSurveyUiDelegate;
    private PropertyModel mModel;
    private SurveyTestTabModelHelper mTabModelHelper;
    private TestMessageDispatcher mTestMessageDispatcher;

    @Mock TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    private boolean mCrashUploadAllowed = true;

    private final CallbackHelper mOnAcceptedCallback = new CallbackHelper();
    private final CallbackHelper mOnDeclinedCallback = new CallbackHelper();
    private final CallbackHelper mOnPresentationFailedCallback = new CallbackHelper();

    @Before
    public void setup() {
        mTabModelHelper = new SurveyTestTabModelHelper();
        mTestMessageDispatcher = new TestMessageDispatcher();
        mModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.CHROME_SURVEY)
                        .build();
        mMessageSurveyUiDelegate =
                new MessageSurveyUiDelegate(
                        mModel,
                        mTestMessageDispatcher,
                        mTabModelSelector,
                        () -> mCrashUploadAllowed);
    }

    @After
    public void tearDown() {
        mTabModelHelper.assertAllObserverDetached();
    }

    @Test
    public void showSurveyInvitationSuccess() {
        mTabModelHelper.skipToReadyForSurvey();
        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(true);

        mTestMessageDispatcher.acceptMessage();
        assertEquals(
                "Delegate state should end at ACCEPTED.",
                State.ACCEPTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals("OnAcceptedCallback is not called.", 1, mOnAcceptedCallback.getCallCount());
    }

    @Test
    public void surveyShownAfterTabLoaded() {
        mTabModelHelper.tabStateInitialized().tabInteractabilityChanged(true);
        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        mTabModelHelper.tabFullyLoaded();
        mTestMessageDispatcher.assertMessageEnqueued(true);
        mMessageSurveyUiDelegate.dismiss();
    }

    @Test
    public void surveyShownAfterTabInteractable() {
        mTabModelHelper.tabStateInitialized().tabFullyLoaded();
        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        mTabModelHelper.tabInteractabilityChanged(true);
        mTestMessageDispatcher.assertMessageEnqueued(true);
        mMessageSurveyUiDelegate.dismiss();
    }

    @Test
    public void dismissMessageWhenTabHidden() {
        mTabModelHelper.tabStateInitialized();
        showSurveyInvitation();

        mTabModelHelper.skipToReadyForSurvey();
        mTestMessageDispatcher.assertMessageEnqueued(true);

        mTabModelHelper.tabHidden(true);
        mTestMessageDispatcher.assertMessageDismissed(DismissReason.TAB_SWITCHED);
        assertEquals("OnDeclinedCallback not called.", 1, mOnDeclinedCallback.getCallCount());
        assertEquals(
                "Delegate state should end at DISMISSED.",
                State.DISMISSED,
                mMessageSurveyUiDelegate.getStateForTesting());
    }

    @Test
    public void dismissMessageByDismissCall() {
        mTabModelHelper.tabStateInitialized();
        showSurveyInvitation();

        mTabModelHelper.skipToReadyForSurvey();
        mTestMessageDispatcher.assertMessageEnqueued(true);

        mMessageSurveyUiDelegate.dismiss();
        mTestMessageDispatcher.assertMessageDismissed(DismissReason.DISMISSED_BY_FEATURE);
        assertEquals(
                "Delegate state should end at DISMISSED.",
                State.DISMISSED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback not called.", 1, mOnDeclinedCallback.getCallCount());
    }

    @Test
    public void cancelWhenCrashUploadDisabled() {
        mTabModelHelper.tabStateInitialized();
        showSurveyInvitation();

        mCrashUploadAllowed = false;
        mTabModelHelper.skipToReadyForSurvey();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "Delegate state should end at NOT_PRESENTED since message is never enqueued.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback not called.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void cancelBeforeRequestShown() {
        assertEquals(
                "Delegate is not started when created.",
                State.NOT_STARTED,
                mMessageSurveyUiDelegate.getStateForTesting());

        mMessageSurveyUiDelegate.dismiss();
        assertEquals(
                "Delegate state should end at NOT_PRESENTED.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback not called, since we never requested.",
                0,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void cancelBeforeEnqueued() {
        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "State should stay at requested while waiting.",
                State.REQUESTED,
                mMessageSurveyUiDelegate.getStateForTesting());

        mMessageSurveyUiDelegate.dismiss();
        assertEquals(
                "Delegate state should end at NOT_PRESENTED.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback should be called after requested.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void cancelWhenTabHiddenWhenLoading() {
        showSurveyInvitation();

        mTabModelHelper.tabStateInitialized().tabHidden(true);
        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "When tab is hidden before presenting the survey, cancel the request.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback is not called.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void cancelBeforeTabFullyLoaded() {
        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "State should stay at requested while waiting.",
                State.REQUESTED,
                mMessageSurveyUiDelegate.getStateForTesting());

        mTabModelHelper.tabStateInitialized();

        mMessageSurveyUiDelegate.dismiss();
        assertEquals(
                "Delegate state should end at NOT_PRESENTED.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "Survey NOT_PRESENTED callback is called after requested.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void cancelBeforeTabIsReadyForSurvey() {
        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "State should stay at requested while waiting.",
                State.REQUESTED,
                mMessageSurveyUiDelegate.getStateForTesting());

        mTabModelHelper.tabStateInitialized().tabFullyLoaded();

        mMessageSurveyUiDelegate.dismiss();
        assertEquals(
                "Delegate state should end at NOT_PRESENTED.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "Survey NOT_PRESENTED callback is called after requested.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void cancelWhenSwitchedIntoIncognito() {
        mTabModelHelper.tabStateInitialized();
        showSurveyInvitation();

        mTabModelHelper.switchToIncognito(true);
        mTabModelHelper.skipToReadyForSurvey();
        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "Delegate state should end at NOT_PRESENTED since message is never enqueued.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback not called.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void noSurveyInvitationInIncognito() {
        mTabModelHelper.switchToIncognito(true);
        showSurveyInvitation();

        mTestMessageDispatcher.assertMessageEnqueued(false);
        assertEquals(
                "Never show survey invitation in incognito.",
                State.NOT_PRESENTED,
                mMessageSurveyUiDelegate.getStateForTesting());
        assertEquals(
                "OnPresentationFailedCallback is not called.",
                1,
                mOnPresentationFailedCallback.getCallCount());
    }

    @Test
    public void noStateUpdateAfterAccepted() {
        mTabModelHelper.skipToReadyForSurvey();
        showSurveyInvitation();
        mTestMessageDispatcher.acceptMessage();
        assertEquals(
                "Delegate state should end as ACCEPTED",
                mMessageSurveyUiDelegate.getStateForTesting(),
                State.ACCEPTED);

        mMessageSurveyUiDelegate.dismiss();
        assertEquals(
                "Delegate state should remain unchanged.",
                mMessageSurveyUiDelegate.getStateForTesting(),
                State.ACCEPTED);
    }

    @Test
    public void noDuplicateShowSurveyInvitation() {
        mTabModelHelper.skipToReadyForSurvey();

        showSurveyInvitation();
        mTestMessageDispatcher.assertMessageEnqueued(true);

        // Try to reuse the delegate to show message again will fail.
        assertThrows(
                "The 2nd #showSurveyInvitation should throw an AssertionError.",
                AssertionError.class,
                this::showSurveyInvitation);
        mMessageSurveyUiDelegate.dismiss();
    }

    @Test
    public void createDefaultMessageModel() {
        PropertyModel model =
                MessageSurveyUiDelegate.populateDefaultValuesForSurveyMessage(
                        ContextUtils.getApplicationContext().getResources(), mModel);

        Resources resources = ContextUtils.getApplicationContext().getResources();
        String defaultTitle = resources.getString(R.string.chrome_survey_message_title);
        String defaultButtonText = resources.getString(R.string.chrome_survey_message_button);
        assertEquals("Title is different.", defaultTitle, model.get(MessageBannerProperties.TITLE));
        assertEquals(
                "Icon resource Id different.",
                R.drawable.fre_product_logo,
                model.get(MessageBannerProperties.ICON_RESOURCE_ID));
        assertEquals(
                "Primary button text is different.",
                defaultButtonText,
                model.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    @Test
    public void createDefaultMessageModelWithValuePopulated() {
        int iconRes = 1235415;
        String myTitle = "my title";
        String myButtonText = "my button text";
        mModel.set(MessageBannerProperties.ICON_RESOURCE_ID, iconRes);
        mModel.set(MessageBannerProperties.TITLE, myTitle);
        mModel.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, myButtonText);
        mModel.set(MessageBannerProperties.DESCRIPTION, "my description");

        MessageSurveyUiDelegate.populateDefaultValuesForSurveyMessage(
                ContextUtils.getApplicationContext().getResources(), mModel);

        assertTrue(mModel.containsKey(MessageBannerProperties.MESSAGE_IDENTIFIER));
        assertEquals("Title is override.", myTitle, mModel.get(MessageBannerProperties.TITLE));
        assertEquals(
                "Icon resource Id is override.",
                iconRes,
                mModel.get(MessageBannerProperties.ICON_RESOURCE_ID));
        assertEquals(
                "Primary button text is override.",
                myButtonText,
                mModel.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    private void showSurveyInvitation() {
        mMessageSurveyUiDelegate.showSurveyInvitation(
                mOnAcceptedCallback::notifyCalled,
                mOnDeclinedCallback::notifyCalled,
                mOnPresentationFailedCallback::notifyCalled);
    }

    private class SurveyTestTabModelHelper {
        private boolean mTabStateInitialized;
        private boolean mTabFullyLoaded;
        private boolean mTabIsHidden;
        private final ObserverList<TabModelSelectorObserver> mTabModelSelectorObserverCaptor =
                new ObserverList<>();
        private final ObserverList<TabObserver> mTabObserverCaptor = new ObserverList<>();

        SurveyTestTabModelHelper() {
            MockitoHelper.doCallback(mTabModelSelectorObserverCaptor::addObserver)
                    .when(mTabModelSelector)
                    .addObserver(any(TabModelSelectorObserver.class));
            MockitoHelper.doCallback(mTabModelSelectorObserverCaptor::removeObserver)
                    .when(mTabModelSelector)
                    .removeObserver(any(TabModelSelectorObserver.class));
            MockitoHelper.doCallback(mTabObserverCaptor::addObserver)
                    .when(mTab)
                    .addObserver(any(TabObserver.class));
            MockitoHelper.doCallback(mTabObserverCaptor::removeObserver)
                    .when(mTab)
                    .removeObserver(any(TabObserver.class));
        }

        SurveyTestTabModelHelper switchToIncognito(boolean isIncognito) {
            doReturn(isIncognito).when(mTabModelSelector).isIncognitoSelected();
            return this;
        }

        SurveyTestTabModelHelper tabStateInitialized() {
            mTabStateInitialized = true;

            doReturn(mTab).when(mTabModelSelector).getCurrentTab();
            doReturn(true).when(mTab).isLoading();
            doReturn(false).when(mTab).isUserInteractable();

            for (var observers : mTabModelSelectorObserverCaptor) {
                observers.onChange();
            }
            return this;
        }

        SurveyTestTabModelHelper tabFullyLoaded() {
            mTabFullyLoaded = true;

            doReturn(false).when(mTab).isLoading();
            for (var observer : mTabObserverCaptor) {
                observer.onLoadStopped(mTab, false);
            }
            return this;
        }

        SurveyTestTabModelHelper tabInteractabilityChanged(boolean isIntractable) {
            doReturn(isIntractable).when(mTab).isUserInteractable();
            for (var observer : mTabObserverCaptor) {
                observer.onInteractabilityChanged(mTab, isIntractable);
            }
            return this;
        }

        SurveyTestTabModelHelper tabHidden(boolean isHidden) {
            if (mTabIsHidden == isHidden) return this;

            mTabIsHidden = isHidden;
            doReturn(isHidden).when(mTab).isHidden();
            for (var observer : mTabObserverCaptor) {
                if (isHidden) {
                    observer.onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
                } else {
                    observer.onShown(mTab, TabSelectionType.FROM_USER);
                }
            }
            return this;
        }

        void skipToReadyForSurvey() {
            if (!mTabStateInitialized) tabStateInitialized();
            if (!mTabFullyLoaded) tabFullyLoaded();

            tabInteractabilityChanged(true).tabHidden(false);
        }

        void assertAllObserverDetached() {
            if (!mTabModelSelectorObserverCaptor.isEmpty()) {
                // Use a mockito verification that should always fail
                verify(
                                mTabModelSelector,
                                never().description("TabModelSelectorObserver is not detached."))
                        .addObserver(any());
            }
            if (!mTabObserverCaptor.isEmpty()) {
                verify(mTab, never().description("TabObserver(s) are not detached."))
                        .addObserver(any());
            }
        }
    }

    private static class TestMessageDispatcher implements MessageDispatcher {
        PropertyModel mModel;
        Integer mDismissedReason;

        @Override
        public void enqueueMessage(
                PropertyModel messageProperties,
                WebContents webContents,
                int scopeType,
                boolean highPriority) {
            throw new UnsupportedOperationException("Should not be used in this test");
        }

        @Override
        public void enqueueWindowScopedMessage(
                PropertyModel messageProperties, boolean highPriority) {
            mModel = messageProperties;
        }

        @Override
        public void dismissMessage(PropertyModel messageProperties, int dismissReason) {
            mModel.get(MessageBannerProperties.ON_DISMISSED).onResult(dismissReason);
            mDismissedReason = dismissReason;
        }

        void acceptMessage() {
            mModel.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
            dismissMessage(mModel, DismissReason.PRIMARY_ACTION);
        }

        void assertMessageEnqueued(boolean hasEnqueued) {
            assertEquals("Message is not enqueued.", hasEnqueued, mModel != null);
        }

        void assertMessageDismissed(int dismissedReason) {
            assertEquals(
                    "Message is not dismissed the same reason.",
                    (int) mDismissedReason,
                    dismissedReason);
        }
    }
}
