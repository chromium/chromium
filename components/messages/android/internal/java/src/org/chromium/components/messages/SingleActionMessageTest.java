// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.test.filters.MediumTest;

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
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link SingleActionMessage}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures({
    MessageFeatureList.MESSAGES_ANDROID_EXTRA_HISTOGRAMS,
    MessageFeatureList.MESSAGES_FOR_ANDROID_FULLY_VISIBLE_CALLBACK,
})
public class SingleActionMessageTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private static class MockDurationProvider implements MessageAutodismissDurationProvider {
        private long mDuration;

        public MockDurationProvider(long duration) {
            mDuration = duration;
        }

        @Override
        public long get(int id, long extension) {
            return mDuration;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();
    @Mock private SwipeAnimationHandler mSwipeAnimationHandler;
    @Mock private MessageBannerCoordinator mMessageBanner;

    private CallbackHelper mPrimaryActionCallback;
    private CallbackHelper mSecondaryActionCallback;
    private CallbackHelper mDismissCallback;
    private SingleActionMessage.DismissCallback mEmptyDismissCallback =
            (model, dismissReason) -> {};

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                });
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
                new SingleActionMessage(
                        container,
                        model,
                        mEmptyDismissCallback,
                        () -> 0,
                        () -> 0,
                        new MockDurationProvider(0L),
                        mSwipeAnimationHandler);
        final MessageBannerView view = createMessageBannerView(container);
        view.setId(R.id.message_banner);
        message.setMessageBannerForTesting(mMessageBanner);
        message.setViewForTesting(view);
        message.show(Position.INVISIBLE, Position.FRONT);
        Assert.assertEquals(
                "Message container should have one message view after the message is shown.",
                1,
                container.getChildCount());
        message.hide(Position.FRONT, Position.INVISIBLE, true);
        // Let's pretend the animation ended, and the mediator called the callback as a result.
        final ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mMessageBanner)
                .hide(
                        eq(Position.FRONT),
                        eq(Position.INVISIBLE),
                        anyBoolean(),
                        runnableCaptor.capture());
        runnableCaptor.getValue().run();
        Assert.assertEquals(
                "Message container should not have any view after the message is hidden.",
                0,
                container.getChildCount());
        message.dismiss(DismissReason.UNKNOWN);
        mDismissCallback.waitForOnly("Dismiss callback should be called when message is dismissed");
        Assert.assertTrue(
                "mMessageDismissed should be true when a message is dismissed.",
                message.getMessageDismissedForTesting());
    }

    @Test
    @MediumTest
    public void testHistogramRecordOnDismiss() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel(MessageIdentifier.SYNC_ERROR);
        PropertyModel m2 = createBasicSingleActionMessageModel(MessageIdentifier.DOWNLOAD_PROGRESS);
        PropertyModel m3 = createBasicSingleActionMessageModel(MessageIdentifier.POPUP_BLOCKED);

        var fullyVisible =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Messages.FullyVisible",
                                MessageIdentifier.SYNC_ERROR,
                                MessageIdentifier.DOWNLOAD_PROGRESS)
                        .build();

        var dismissal =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Messages.DismissedWithoutFullyVisible",
                                MessageIdentifier.POPUP_BLOCKED)
                        .expectIntRecord("Android.Messages.TimeToFullyShow.SyncError", 1000)
                        .expectIntRecord("Android.Messages.TimeToFullyShow.DownloadProgress", 1500)
                        .build();

        final MessageBannerView view1 = createMessageBannerView(container);
        final MessageBannerView view2 = createMessageBannerView(container);
        final MessageBannerView view3 = createMessageBannerView(container);
        var sam1 = createSingleActionMessage(container, m1, view1);
        var sam2 = createSingleActionMessage(container, m2, view2);
        var sam3 = createSingleActionMessage(container, m3, view3);

        // dismiss without showing
        sam3.dismiss(DismissReason.DISMISSED_BY_FEATURE);

        mFakeTime.advanceMillis(1000);
        sam1.show(Position.INVISIBLE, Position.FRONT);
        sam2.show(Position.FRONT, Position.BACK);

        // to test this will not trigger a recordation of DismissedWithoutFullyVisible.
        sam1.dismiss(DismissReason.GESTURE);

        mFakeTime.advanceMillis(500);
        // move to front to make sam2 also fully visible.
        sam2.show(Position.BACK, Position.FRONT);

        fullyVisible.assertExpected("Messages should have been fully visible before");
        dismissal.assertExpected("Histograms are not recorded when a message is dismissed");
    }

    @Test
    @MediumTest
    public void testAutoDismissDuration() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        long duration = 42;
        SingleActionMessage message =
                new SingleActionMessage(
                        container,
                        model,
                        mEmptyDismissCallback,
                        () -> 0,
                        () -> 0,
                        new MockDurationProvider(duration),
                        mSwipeAnimationHandler);
        Assert.assertEquals(
                "Autodismiss duration is not propagated correctly.",
                duration,
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
                new SingleActionMessage(
                        container,
                        model,
                        mEmptyDismissCallback,
                        () -> 0,
                        () -> 0,
                        new MockDurationProvider(duration + 1000),
                        mSwipeAnimationHandler);
        Assert.assertEquals(
                "Autodismiss duration is not propagated correctly.",
                duration + 1000,
                message.getAutoDismissDuration());
    }

    @Test
    @MediumTest
    public void testAddMultipleSingleActionMessage() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel();
        PropertyModel m2 = createBasicSingleActionMessageModel();
        final MessageBannerView view1 = createMessageBannerView(container);
        final MessageBannerView view2 = createMessageBannerView(container);
        // expect no crash
        createAndShowSingleActionMessage(container, m1, view1);
        createAndShowSingleActionMessage(container, m2, view2);
    }

    @Test
    @MediumTest
    public void testAddAndRemoveSingleActionMessage_withStacking() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel();
        PropertyModel m2 = createBasicSingleActionMessageModel();
        final MessageBannerView view1 = createMessageBannerView(container);
        final MessageBannerView view2 = createMessageBannerView(container);
        createAndShowSingleActionMessage(container, m1, view1, Position.INVISIBLE, Position.FRONT);
        createAndShowSingleActionMessage(container, m2, view2, Position.FRONT, Position.BACK);
        Assert.assertTrue(
                "front view's elevation "
                        + view1.getElevation()
                        + " should be larger than the back one "
                        + view2.getElevation(),
                view1.getElevation() > view2.getElevation());

        PropertyModel m3 = createBasicSingleActionMessageModel();
        final MessageBannerView view3 = createMessageBannerView(container);
        container.removeMessage(view1);
        createAndShowSingleActionMessage(container, m3, view3, Position.INVISIBLE, Position.FRONT);
        Assert.assertTrue(
                "front view's elevation "
                        + view3.getElevation()
                        + " should be larger than the back one "
                        + view2.getElevation(),
                view3.getElevation() > view2.getElevation());
    }

    @Test(expected = IllegalStateException.class)
    @MediumTest
    public void testAddMultipleSingleActionMessage_withStacking() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel();
        PropertyModel m2 = createBasicSingleActionMessageModel();
        PropertyModel m3 = createBasicSingleActionMessageModel();
        final MessageBannerView view1 = createMessageBannerView(container);
        final MessageBannerView view2 = createMessageBannerView(container);
        final MessageBannerView view3 = createMessageBannerView(container);
        createAndShowSingleActionMessage(container, m1, view1, Position.INVISIBLE, Position.FRONT);
        createAndShowSingleActionMessage(container, m2, view2, Position.FRONT, Position.BACK);
        createAndShowSingleActionMessage(container, m3, view3, Position.FRONT, Position.BACK);
    }

    @Test
    @MediumTest
    public void testPrimaryActionCallbackInvokedOnce() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        final MessageBannerView view = createMessageBannerView(container);
        SingleActionMessage message = createAndShowSingleActionMessage(container, model, view);
        executeAndVerifyRepeatedButtonClicks(true, model, message, view);
    }

    @Test
    @MediumTest
    public void testSecondaryActionCallbackInvokedOnce() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel model = createBasicSingleActionMessageModel();
        final MessageBannerView view = createMessageBannerView(container);
        SingleActionMessage message = createAndShowSingleActionMessage(container, model, view);
        executeAndVerifyRepeatedButtonClicks(false, model, message, view);
    }

    @Test
    @MediumTest
    public void testOnFullyVisible() {
        MessageContainer container = new MessageContainer(sActivity, null);
        PropertyModel m1 = createBasicSingleActionMessageModel(1);
        PropertyModel m2 = createBasicSingleActionMessageModel(2);
        Callback<Boolean> callback1 = Mockito.mock(Callback.class);
        m1.set(MessageBannerProperties.ON_FULLY_VISIBLE, callback1);
        Callback<Boolean> callback2 = Mockito.mock(Callback.class);
        m2.set(MessageBannerProperties.ON_FULLY_VISIBLE, callback2);

        var fullyVisible =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Android.Messages.FullyVisible", 1, 2)
                        .build();

        final MessageBannerView view1 = createMessageBannerView(container);
        final MessageBannerView view2 = createMessageBannerView(container);
        var sam1 =
                createAndShowSingleActionMessage(
                        container, m1, view1, Position.INVISIBLE, Position.FRONT);
        var sam2 =
                createAndShowSingleActionMessage(
                        container, m2, view2, Position.FRONT, Position.BACK);

        verify(callback1).onResult(true);
        verify(callback2, never()).onResult(anyBoolean());

        sam1.hide(Position.FRONT, Position.INVISIBLE, false);
        verify(callback1).onResult(false);

        sam2.show(Position.BACK, Position.FRONT);
        verify(callback2).onResult(true);
        fullyVisible.assertExpected("Messages should have been fully visible before");
    }

    private void executeAndVerifyRepeatedButtonClicks(
            boolean isPrimaryButtonClickedFirst,
            PropertyModel model,
            SingleActionMessage message,
            MessageBannerView view) {
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
        Assert.assertTrue(
                "mMessageDismissed should be true when a message is dismissed.",
                message.getMessageDismissedForTesting());
        // Simulate subsequent button clicks.
        model.get(MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER).onClick(view);
        model.get(MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK).run();
        Assert.assertEquals(
                "The primary action callback was not run the expected number of times.",
                expectedPrimaryActionCallbackCount,
                mPrimaryActionCallback.getCallCount());
        Assert.assertEquals(
                "The secondary action callback was not run the expected number of times.",
                expectedSecondaryActionCallbackCount,
                mSecondaryActionCallback.getCallCount());
    }

    private SingleActionMessage createSingleActionMessage(
            MessageContainer container, PropertyModel model, MessageBannerView view) {
        SingleActionMessage message =
                new SingleActionMessage(
                        container,
                        model,
                        mEmptyDismissCallback,
                        () -> 0,
                        () -> 0,
                        new MockDurationProvider(0L),
                        mSwipeAnimationHandler);
        view.setId(R.id.message_banner);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        message.setMessageBannerForTesting(mMessageBanner);
        message.setViewForTesting(view);
        return message;
    }

    private SingleActionMessage createAndShowSingleActionMessage(
            MessageContainer container,
            PropertyModel model,
            MessageBannerView view,
            @Position int from,
            @Position int to) {
        SingleActionMessage message = createSingleActionMessage(container, model, view);
        message.show(from, to);
        return message;
    }

    private SingleActionMessage createAndShowSingleActionMessage(
            MessageContainer container, PropertyModel model, MessageBannerView view) {
        return createAndShowSingleActionMessage(
                container, model, view, Position.INVISIBLE, Position.FRONT);
    }

    private MessageBannerView createMessageBannerView(MessageContainer container) {
        return (MessageBannerView)
                LayoutInflater.from(container.getContext())
                        .inflate(R.layout.message_banner_view, container, false);
    }

    private PropertyModel createBasicSingleActionMessageModel(int id) {
        return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                .with(MessageBannerProperties.MESSAGE_IDENTIFIER, id)
                .with(MessageBannerProperties.TITLE, "test")
                .with(MessageBannerProperties.DESCRIPTION, "Description")
                .with(
                        MessageBannerProperties.ICON,
                        ApiCompatibilityUtils.getDrawable(
                                sActivity.getResources(), android.R.drawable.ic_menu_add))
                .with(
                        MessageBannerProperties.ON_PRIMARY_ACTION,
                        () -> {
                            mPrimaryActionCallback.notifyCalled();
                            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                        })
                .with(
                        MessageBannerProperties.ON_SECONDARY_ACTION,
                        () -> {
                            mSecondaryActionCallback.notifyCalled();
                        })
                .with(MessageBannerProperties.ON_TOUCH_RUNNABLE, () -> {})
                .with(
                        MessageBannerProperties.ON_DISMISSED,
                        (dismissReason) -> {
                            mDismissCallback.notifyCalled();
                        })
                .build();
    }

    private PropertyModel createBasicSingleActionMessageModel() {
        return createBasicSingleActionMessageModel(MessageIdentifier.TEST_MESSAGE);
    }
}
