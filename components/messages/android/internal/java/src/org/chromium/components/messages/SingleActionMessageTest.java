// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.app.Activity;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Tests for {@link SingleActionMessage}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SingleActionMessageTest {
    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private class MockDurationProvider implements MessageAutodismissDurationProvider {
        private long mDuration;
        public MockDurationProvider(long duration) {
            mDuration = duration;
        }

        @Override
        public long get(int id, long extension) {
            return mDuration;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private Callback<Animator> mAnimatorStartCallback;

    private CallbackHelper mPrimaryActionCallback;
    private CallbackHelper mSecondaryActionCallback;
    private CallbackHelper mDismissCallback;
    private SingleActionMessage.DismissCallback mEmptyDismissCallback =
            (model, dismissReason) -> {};

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sActivity = sActivityTestRule.getActivity(); });
    }

    @Before
    public void setupTest() throws Exception {
        mDismissCallback = new CallbackHelper();
        mPrimaryActionCallback = new CallbackHelper();
        mSecondaryActionCallback = new CallbackHelper();
    }

    @Test
    @MediumTest
    public void testAddAndRemoveSingleActionMessage() throws Exception {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        SingleActionMessage message =
                new SingleActionMessage(container, model, mEmptyDismissCallback,
                        () -> 0, new MockDurationProvider(0L), mAnimatorStartCallback);
        final MessageBannerCoordinator messageBanner = Mockito.mock(MessageBannerCoordinator.class);
        final MessageBannerView view = new MessageBannerView(sActivity, null);
        view.setId(R.id.message_banner);
        message.setMessageBannerForTesting(messageBanner);
        message.setViewForTesting(view);
        message.show();
        Assert.assertEquals(
                "Message container should have one message view after the message is shown.", 1,
                container.getChildCount());
        message.hide(true, () -> {});
        // Let's pretend the animation ended, and the mediator called the callback as a result.
        final ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(messageBanner).hide(anyBoolean(), runnableCaptor.capture());
        runnableCaptor.getValue().run();
        Assert.assertEquals(
                "Message container should not have any view after the message is hidden.", 0,
                container.getChildCount());
        message.dismiss(DismissReason.UNKNOWN);
        mDismissCallback.waitForFirst(
                "Dismiss callback should be called when message is dismissed");
        Assert.assertTrue("mMessageDismissed should be true when a message is dismissed.",
                message.getMessageDismissedForTesting());
    }

    @Test
    @MediumTest
    public void testAutoDismissDuration() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        long duration = 42;
        SingleActionMessage message =
                new SingleActionMessage(container, model, mEmptyDismissCallback,
                        () -> 0, new MockDurationProvider(duration), mAnimatorStartCallback);
        Assert.assertEquals("Autodismiss duration is not propagated correctly.", duration,
                message.getAutoDismissDuration());
    }

    @Test
    @MediumTest
    public void testAutoDismissDurationExtended() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        model.set(MessageBannerProperties.DISMISSAL_DURATION, 1000);
        long duration = 42;
        SingleActionMessage message =
                new SingleActionMessage(container, model, mEmptyDismissCallback,
                        () -> 0, new MockDurationProvider(duration + 1000), mAnimatorStartCallback);
        Assert.assertEquals("Autodismiss duration is not propagated correctly.", duration + 1000,
                message.getAutoDismissDuration());
    }

    @Test(expected = IllegalStateException.class)
    @MediumTest
    public void testAddMultipleSingleActionMessage() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel();
        PropertyModel m2 = createBasicSingleActionMessageModel();
        final MessageBannerView view1 = new MessageBannerView(sActivity, null);
        final MessageBannerView view2 = new MessageBannerView(sActivity, null);
        createAndShowSingleActionMessage(container, m1, view1);
        createAndShowSingleActionMessage(container, m2, view2);
    }

    @Test
    @MediumTest
    public void testPrimaryActionCallbackInvokedOnce() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        final MessageBannerView view = new MessageBannerView(sActivity, null);
        SingleActionMessage message = createAndShowSingleActionMessage(container, model, view);
        executeAndVerifyRepeatedButtonClicks(true, model, message, view);
    }

    @Test
    @MediumTest
    public void testSecondaryActionCallbackInvokedOnce() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        final MessageBannerView view = new MessageBannerView(sActivity, null);
        SingleActionMessage message = createAndShowSingleActionMessage(container, model, view);
        executeAndVerifyRepeatedButtonClicks(false, model, message, view);
    }

    @Test
    @SmallTest
    public void testMessageShouldShowDefault() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        final MessageBannerView view = new MessageBannerView(sActivity, null);
        SingleActionMessage message = createAndShowSingleActionMessage(container, model, view);
        Assert.assertTrue("#shouldShow should be true by default.", message.shouldShow());
    }

    @Test
    @SmallTest
    public void testMessageShouldNotShow() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        model.set(MessageBannerProperties.ON_STARTED_SHOWING, () -> false);
        final MessageBannerView view = new MessageBannerView(sActivity, null);
        SingleActionMessage message = createAndShowSingleActionMessage(container, model, view);
        Assert.assertFalse(
                "#shouldShow should be false when the ON_STARTED_SHOWING supplier returns false.",
                message.shouldShow());
    }

    private void executeAndVerifyRepeatedButtonClicks(boolean isPrimaryButtonClickedFirst,
            PropertyModel model, SingleActionMessage message, MessageBannerView view) {
        int expectedPrimaryActionCallbackCount = mPrimaryActionCallback.getCallCount();
        int expectedSecondaryActionCallbackCount = mSecondaryActionCallback.getCallCount();
        if (isPrimaryButtonClickedFirst) {
            model.get(MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER).onClick(view);
            expectedPrimaryActionCallbackCount += 1;
        } else {
            model.get(MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK).run();
            expectedSecondaryActionCallbackCount += 1;
        }
        // Simulate message dismissal on button click.
        message.dismiss(DismissReason.UNKNOWN);
        Assert.assertTrue("mMessageDismissed should be true when a message is dismissed.",
                message.getMessageDismissedForTesting());
        // Simulate subsequent button clicks.
        model.get(MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER).onClick(view);
        model.get(MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK).run();
        Assert.assertEquals("The primary action callback was not run the expected number of times.",
                expectedPrimaryActionCallbackCount, mPrimaryActionCallback.getCallCount());
        Assert.assertEquals(
                "The secondary action callback was not run the expected number of times.",
                expectedSecondaryActionCallbackCount, mSecondaryActionCallback.getCallCount());
    }

    private SingleActionMessage createAndShowSingleActionMessage(
            MessageContainer container, PropertyModel model, MessageBannerView view) {
        SingleActionMessage message =
                new SingleActionMessage(container, model, mEmptyDismissCallback,
                        () -> 0, new MockDurationProvider(0L), mAnimatorStartCallback);
        final MessageBannerCoordinator messageBanner = Mockito.mock(MessageBannerCoordinator.class);
        view.setId(R.id.message_banner);
        message.setMessageBannerForTesting(messageBanner);
        message.setViewForTesting(view);
        message.show();
        return message;
    }

    private PropertyModel createBasicSingleActionMessageModel() {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.MESSAGE_IDENTIFIER, MessageIdentifier.TEST_MESSAGE)
                .with(MessageBannerProperties.TITLE, "test")
                .with(MessageBannerProperties.DESCRIPTION, "Description")
                .with(MessageBannerProperties.ICON,
                        ApiCompatibilityUtils.getDrawable(
                                sActivity.getResources(), android.R.drawable.ic_menu_add))
                .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                        () -> {
                            mPrimaryActionCallback.notifyCalled();
                            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                        })
                .with(MessageBannerProperties.ON_SECONDARY_ACTION,
                        () -> { mSecondaryActionCallback.notifyCalled(); })
                .with(MessageBannerProperties.ON_TOUCH_RUNNABLE, () -> {})
                .with(MessageBannerProperties.ON_DISMISSED,
                        (dismissReason) -> { mDismissCallback.notifyCalled(); })
                .build();
    }
}
