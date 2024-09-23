// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static android.os.Looper.getMainLooper;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;
import static org.robolectric.annotation.LooperMode.Mode.PAUSED;

import android.animation.Animator;
import android.animation.ValueAnimator;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageStateHandler.Position;

import java.util.Arrays;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link MessageAnimationCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(PAUSED)
public class MessageAnimationCoordinatorUnitTest {
    private MessageQueueDelegate mQueueDelegate =
            Mockito.spy(
                    new MessageQueueDelegate() {
                        @Override
                        public void onRequestShowing(Runnable callback) {
                            callback.run();
                        }

                        @Override
                        public void onFinishHiding() {}

                        @Override
                        public void onAnimationStart() {}

                        @Override
                        public void onAnimationEnd() {}

                        @Override
                        public boolean isReadyForShowing() {
                            return true;
                        }

                        @Override
                        public boolean isPendingShow() {
                            return false;
                        }

                        @Override
                        public boolean isDestroyed() {
                            return false;
                        }

                        @Override
                        public boolean isSwitchingScope() {
                            return false;
                        }
                    });

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MessageContainer mContainer;

    @Mock private Callback<Animator> mAnimatorStartCallback;

    private MessageAnimationCoordinator mAnimationCoordinator;

    @Before
    public void setUp() {
        var testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                MessageFeatureList.MESSAGES_ANDROID_EXTRA_HISTOGRAMS, true);
        FeatureList.setTestValues(testValues);
        mAnimationCoordinator = new MessageAnimationCoordinator(mContainer, Animator::start);
        mAnimationCoordinator.setMessageQueueDelegate(mQueueDelegate);
        when(mContainer.isIsInitializingLayout()).thenReturn(false);
    }

    // Test incoming candidates are same with current displayed ones.
    // [m1, m2] -> [m1, m2]
    @Test
    @SmallTest
    public void testDoNothing() throws java.util.concurrent.TimeoutException {
        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        // Initial setup
        CallbackHelper callbackHelper = new CallbackHelper();
        mAnimationCoordinator.updateWithStacking(
                Arrays.asList(m1, m2), false, callbackHelper::notifyCalled);
        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mContainer).runAfterInitialMessageLayout(runnableCaptor.capture());
        runnableCaptor.getValue().run();
        verify(mQueueDelegate).onAnimationStart();
        shadowOf(getMainLooper()).idle();
        callbackHelper.waitForOnly();
        verify(mQueueDelegate).onAnimationEnd();

        // Again with same candidates.
        mAnimationCoordinator.updateWithStacking(
                Arrays.asList(m1, m2),
                false,
                () -> {
                    verify(mQueueDelegate, times(1).description("Should not be called again"))
                            .onAnimationEnd();
                });
        verify(mQueueDelegate, times(1).description("Should not be called again"))
                .onAnimationStart();

        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m1.handler).show(anyInt(), anyInt());
        verify(m2.handler).show(anyInt(), anyInt());
    }

    // Test showing messages.
    // [null, null] -> [m1, m2]
    @Test
    @SmallTest
    public void testShowMessages() {
        // Initial values should be null.
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.SHOW_ALL)
                        .expectIntRecord("Android.Messages.Stacking.RequestToFullyShow", 1)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.Hiding", 1)
                        .expectIntRecord(
                                "Android.Messages.Stacking.InsertAtBack",
                                MessageIdentifier.COUNT - 1)
                        .expectIntRecord(
                                "Android.Messages.Stacking.Hidden", MessageIdentifier.COUNT - 1)
                        .build();
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, MessageIdentifier.COUNT - 1);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler, never()).hide(anyInt(), anyInt(), anyBoolean());

        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
        histogramWatcher.assertExpected();
    }

    // Test only front message becomes hidden.
    // [m1, m2] -> [m2, null]
    @Test
    @SmallTest
    public void testHideFrontMessageOnly() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);

        var requestToFullyShow =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Stacking.RequestToFullyShow", 1);

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});
        requestToFullyShow.assertExpected("M1 is not fully shown.");

        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.REMOVE_FRONT_AND_SHOW_BACK)
                        .expectIntRecord("Android.Messages.Stacking.RequestToFullyShow", 2)
                        .expectIntRecord("Android.Messages.Stacking.RemoveFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.PushToFront", 2)
                        .expectNoRecords("Android.Messages.Stacking.Hidden")
                        .expectNoRecords("Android.Messages.Stacking.Hiding")
                        .build();
        // Hide the front one so that the back one is brought to front.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, null), false, () -> {});
        inOrder.verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        inOrder.verify(m2.handler).show(Position.BACK, Position.FRONT);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());
        histogramWatcher.assertExpected();
    }

    // Test hiding front one and then showing a new one.
    // [m1, m2] -> [m2, m3] is done in two steps.
    // [m1, m2] -> [m2, null], then [m2, null] -> [m2, m3]
    @Test
    @SmallTest
    public void testDismissFrontAndEnqueueNew() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        var requestToFullyShow =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Stacking.RequestToFullyShow", 1);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        requestToFullyShow.assertExpected("M1 is not fully shown");
        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        MessageState m3 = buildMessageState();
        setMessageIdentifier(m3, 3);

        requestToFullyShow =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Stacking.RequestToFullyShow", 2);
        // Hide the front one so that the back one is brought to front.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m3), false, () -> {});
        inOrder.verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        inOrder.verify(m2.handler).show(Position.BACK, Position.FRONT);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());
        requestToFullyShow.assertExpected("M2 is not fully shown");

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.SHOW_BACK_ONLY)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtBack", 3)
                        .expectIntRecord("Android.Messages.Stacking.Hiding", 2)
                        .expectIntRecord("Android.Messages.Stacking.Hidden", 3)
                        .build();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m3), false, () -> {});
        verify(m3.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, m3}, currentMessages.toArray());
        histogramWatcher.assertExpected();
    }

    // Test only back message becomes hidden.
    // [m1, m2] -> [m1, null]
    @Test
    @SmallTest
    public void testHiddenBackMessageOnly() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        var requestToFullyShow =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Stacking.RequestToFullyShow", 1);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        requestToFullyShow.assertExpected("M1 is not fully shown");
        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.REMOVE_BACK_ONLY)
                        .expectIntRecord("Android.Messages.Stacking.RemoveBack", 2)
                        .expectNoRecords("Android.Messages.Stacking.Hidden")
                        .expectNoRecords("Android.Messages.Stacking.Hiding")
                        // do not trigger again as m1 stays in the foreground
                        .expectNoRecords("Android.Messages.Stacking.RequestToFullyShow")
                        .build();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});
        inOrder.verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        inOrder.verify(m2.handler).hide(Position.BACK, Position.FRONT, true);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, null}, currentMessages.toArray());
        histogramWatcher.assertExpected();
    }

    /** Test replacing back message. [m1, m2] -> [m1, m3] */
    @Test
    @SmallTest
    public void testReplacingBack() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        MessageState m3 = buildMessageState();
        setMessageIdentifier(m3, 3);

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});
        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        // When transiting from [m1, m2] -> [m1, m3], finish this into two steps:
        // [m1, m2] -> [m1, null] -> [m1, m3]
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m3), false, () -> {});
        verify(m2.handler).hide(eq(Position.BACK), eq(Position.FRONT), anyBoolean());
        verify(m3.handler, never()).show(anyInt(), anyInt());
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, null}, currentMessages.toArray());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m3), false, () -> {});
        verify(m3.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m3}, currentMessages.toArray());
    }

    // Test pushing front message to back.
    // [m1, null] -> [m2, m1]
    @Test
    @SmallTest
    public void testPushFrontBack() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});

        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler, never()).show(Position.FRONT, Position.BACK);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.Messages.Stacking.Hiding", 2)
                        .expectIntRecord("Android.Messages.Stacking.Hidden", 1)
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.INSERT_AT_FRONT)
                        .expectIntRecord("Android.Messages.Stacking.PushToBack", 1)
                        .build();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m1), false, () -> {});
        inOrder.verify(m2.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m1.handler).show(Position.FRONT, Position.BACK);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, m1}, currentMessages.toArray());
        histogramWatcher.assertExpected();
    }

    // Test pushing front message to back.
    // [m1, null] -> [m2, null]
    // TODO(crbug.com/40877229): simplify this into one step.
    // This should be done in two steps:  [m1, null] -> [null, null] -> [m2, null]
    @Test
    @SmallTest
    public void testUpdateFrontMessageOnly() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler, never()).show(Position.FRONT, Position.BACK);

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, null), false, () -> {});
        verify(m2.handler, never()).show(anyInt(), anyInt());
        verify(m1.handler).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, null), false, () -> {});
        verify(m2.handler).show(Position.INVISIBLE, Position.FRONT);
        // Do not trigger #hide again.
        verify(m1.handler, times(1)).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());
    }

    /** Test messages are hidden before #onStartShow is done. */
    @Test
    @SmallTest
    public void testHideBeforeFullyShow() {
        mAnimationCoordinator.setMessageQueueDelegate(
                new MessageQueueDelegate() {
                    @Override
                    public void onRequestShowing(Runnable callback) {}

                    @Override
                    public void onFinishHiding() {}

                    @Override
                    public void onAnimationStart() {}

                    @Override
                    public void onAnimationEnd() {}

                    @Override
                    public boolean isReadyForShowing() {
                        return true;
                    }

                    @Override
                    public boolean isPendingShow() {
                        return false;
                    }

                    @Override
                    public boolean isDestroyed() {
                        return false;
                    }

                    @Override
                    public boolean isSwitchingScope() {
                        return false;
                    }
                });
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.SHOW_ALL)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtBack", 2)
                        .expectIntRecord("Android.Messages.Stacking.Hiding", 1)
                        .expectIntRecord("Android.Messages.Stacking.Hidden", 2)
                        .build();
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        histogramWatcher.assertExpected();
        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), false, () -> {});
        verify(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).hide(anyInt(), anyInt(), anyBoolean());
    }

    // Test showing animation is triggered after hiding animation is started.
    @Test
    @SmallTest
    public void testObsoleteShowingAnimation() {
        mAnimationCoordinator = new MessageAnimationCoordinator(mContainer, mAnimatorStartCallback);
        mAnimationCoordinator.setMessageQueueDelegate(mQueueDelegate);
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.SHOW_ALL)
                        .expectIntRecord("Android.Messages.Stacking.Hiding", 1)
                        .expectIntRecord("Android.Messages.Stacking.Hidden", 2)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtBack", 2)
                        .build();
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});
        verify(mContainer).runAfterInitialMessageLayout(captor.capture());
        histogramWatcher.assertExpected();
        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
        verify(mAnimatorStartCallback, never()).onResult(any());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), false, () -> {});
        verify(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(mQueueDelegate, times(1)).onAnimationStart();
        verify(mAnimatorStartCallback, times(1)).onResult(any());

        // Trigger showing animation after hiding animation is started.
        captor.getValue().run();
        verify(mQueueDelegate, times(1)).onAnimationStart();
        verify(mAnimatorStartCallback, times(1)).onResult(any());
    }

    // Test when onStartShowing takes some time to be ready.
    @Test
    @SmallTest
    public void testWaitForOnStartShowing() {
        mAnimationCoordinator = new MessageAnimationCoordinator(mContainer, mAnimatorStartCallback);
        MessageQueueDelegate queueDelegate = Mockito.mock(MessageQueueDelegate.class);
        when(queueDelegate.isReadyForShowing()).thenReturn(false);
        mAnimationCoordinator.setMessageQueueDelegate(queueDelegate);
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.SHOW_ALL)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtBack", 2)
                        .expectIntRecord("Android.Messages.Stacking.BlockedByBrowserControl", 1)
                        .build();
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        mAnimationCoordinator.updateWithStacking(
                Arrays.asList(m1, m2),
                false,
                () -> {
                    when(queueDelegate.isReadyForShowing()).thenReturn(true);
                    Assert.assertArrayEquals(
                            new MessageState[] {null, null},
                            mAnimationCoordinator.getCurrentDisplayedMessages().toArray());
                    mAnimationCoordinator.updateWithStacking(
                            Arrays.asList(m1, m2), false, () -> {});
                });
        verify(queueDelegate).onRequestShowing(captor.capture());
        captor.getValue().run();
        histogramWatcher.assertExpected();
        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
        verify(mAnimatorStartCallback, never()).onResult(any());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), false, () -> {});
        verify(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(queueDelegate, times(1)).onAnimationStart();
        verify(mAnimatorStartCallback, times(1)).onResult(any());

        verify(mContainer).runAfterInitialMessageLayout(captor.capture());
        captor.getValue().run();
        verify(queueDelegate, times(1)).onAnimationStart();
        verify(mAnimatorStartCallback, times(1)).onResult(any());
    }

    // Test a new message is enqueued when the previous message is still waiting for onStartShowing.
    @Test
    @SmallTest
    public void testEnqueuingWhileWaitingForOnStartShowing() {
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mContainer)
                .runAfterInitialMessageLayout(any(Runnable.class));
        mAnimationCoordinator = new MessageAnimationCoordinator(mContainer, mAnimatorStartCallback);
        MessageQueueDelegate queueDelegate = Mockito.mock(MessageQueueDelegate.class);
        when(queueDelegate.isReadyForShowing()).thenReturn(false);
        mAnimationCoordinator.setMessageQueueDelegate(queueDelegate);
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        mAnimationCoordinator.updateWithStacking(
                Arrays.asList(m1, null),
                false,
                () -> {
                    when(queueDelegate.isReadyForShowing()).thenReturn(true);
                    Assert.assertArrayEquals(
                            new MessageState[] {null, null},
                            mAnimationCoordinator.getCurrentDisplayedMessages().toArray());
                    mAnimationCoordinator.updateWithStacking(
                            Arrays.asList(m1, m2), false, () -> {});
                });

        var blockedByBrowserControl =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Stacking.BlockedByBrowserControl", 1);

        // M1 is waiting to be shown.
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());

        verify(queueDelegate).onRequestShowing(captor.capture());

        // Before onStartShowing is finished, m2 is enqueued.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        // Nothing happens, as message queue is not ready yet.
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        blockedByBrowserControl.assertExpected("Messages should be blocked by browser control.");

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.SHOW_ALL)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.InsertAtBack", 2)
                        .build();
        // onStartShowing is finished. Showing two messages at the same time.
        captor.getValue().run();

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
        verify(mAnimatorStartCallback).onResult(any());

        histogramWatcher.assertExpected("Stacking histogram not correctly recorded during showing");

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.Messages.Stacking",
                                MessagesMetrics.StackingAnimationType.REMOVE_ALL)
                        .expectIntRecord("Android.Messages.Stacking.RemoveFront", 1)
                        .expectIntRecord("Android.Messages.Stacking.RemoveBack", 2)
                        .build();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), false, () -> {});
        verify(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(queueDelegate, times(2)).onAnimationStart();
        verify(mAnimatorStartCallback, times(2)).onResult(any());
        histogramWatcher.assertExpected("Stacking histogram not correctly recorded during hiding");
    }

    // Test when suspension cancels a hiding animation.
    @Test
    @SmallTest
    public void testSuspensionCancellingHidingAnimation() throws TimeoutException {
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mContainer)
                .runAfterInitialMessageLayout(any(Runnable.class));
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});
        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
        shadowOf(getMainLooper()).idle();

        var animator = ValueAnimator.ofInt(0, 1);
        animator.setDuration(100000);
        doReturn(animator).when(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, null), false, () -> {});
        verify(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).show(Position.BACK, Position.FRONT);

        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());
        Assert.assertTrue(mAnimationCoordinator.getAnimatorSetForTesting().isStarted());

        mAnimationCoordinator.updateWithStacking(
                Arrays.asList(null, null),
                true,
                () -> {
                    // Simulate triggering the callback given by MessageQueueManager after animation
                    // is finished; equivalent to calling MessageQueueManager's
                    // updateWithStacking.
                    mAnimationCoordinator.updateWithStacking(
                            Arrays.asList(null, null), true, () -> {});
                });
        Assert.assertFalse(mAnimationCoordinator.getAnimatorSetForTesting().isStarted());
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
    }

    // Test that second message should not trigger new animation if the first message is still
    // waiting for container to finish layout.
    @Test
    @SmallTest
    public void testContainerIsInitializingLayout() {
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        doReturn(false).when(mContainer).runAfterInitialMessageLayout(any());
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Stacking.BlockedByContainerNotInitialized", 1);

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});

        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler, never()).show(Position.FRONT, Position.BACK);
        verify(mContainer).runAfterInitialMessageLayout(any());

        when(mContainer.isIsInitializingLayout()).thenReturn(true);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});
        // Second message should not trigger a new animation if the first message is still
        // waiting for container to finish layout.
        verify(m2.handler, never()).show(anyInt(), anyInt());

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, null}, currentMessages.toArray());
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testCurrentMessagesUpdateWhileWaitingForDelegateBeReady() {
        when(mQueueDelegate.isReadyForShowing()).thenReturn(false);
        doNothing().when(mQueueDelegate).onRequestShowing(any());
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});
        verify(mQueueDelegate).onRequestShowing(any());
        // Queue is updated while queue is still waiting the queue to be ready.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), true, () -> {});

        // Queue becomes resumed again and the delegate is ready for showing.
        when(mQueueDelegate.isReadyForShowing()).thenReturn(true);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), false, () -> {});
        verify(mQueueDelegate).onFinishHiding();
    }

    @Test
    @SmallTest
    public void testUpdateAfterLifecycleDestroyed() {
        when(mQueueDelegate.isReadyForShowing()).thenReturn(false);
        when(mQueueDelegate.isDestroyed()).thenReturn(true);
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});
        verify(mQueueDelegate, never()).onRequestShowing(any());
    }

    private void setMessageIdentifier(MessageState message, int messageIdentifier) {
        doReturn(messageIdentifier).when(message.handler).getMessageIdentifier();
    }

    private MessageState buildMessageState() {
        var handler = Mockito.mock(MessageStateHandler.class);
        doReturn(ValueAnimator.ofInt(1, 2)).when(handler).show(anyInt(), anyInt());
        doReturn(null).when(handler).hide(anyInt(), anyInt(), anyBoolean());
        return new MessageState(null, null, handler, false);
    }
}
