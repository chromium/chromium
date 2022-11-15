// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static android.os.Looper.getMainLooper;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;
import static org.robolectric.annotation.LooperMode.Mode.PAUSED;

import android.animation.Animator;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageStateHandler.Position;

import java.util.Arrays;

/**
 * Unit tests for {@link MessageAnimationCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(PAUSED)
public class MessageAnimationCoordinatorUnitTest {
    private MessageQueueDelegate mQueueDelegate = Mockito.spy(new MessageQueueDelegate() {
        @Override
        public void onStartShowing(Runnable callback) {
            callback.run();
        }

        @Override
        public void onFinishHiding() {}

        @Override
        public void onAnimationStart() {}

        @Override
        public void onAnimationEnd() {}
    });

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MessageContainer mContainer;

    @Mock
    private Callback<Animator> mAnimatorStartCallback;

    private MessageAnimationCoordinator mAnimationCoordinator;

    @Before
    public void setUp() {
        mAnimationCoordinator = new MessageAnimationCoordinator(mContainer, Animator::start);
        doNothing().when(mAnimatorStartCallback).onResult(any());
        mAnimationCoordinator.setMessageQueueDelegate(mQueueDelegate);
    }

    @Test
    @SmallTest
    public void testDoNothing_withoutStacking() {
        MessageState m1 = buildMessageState();

        // Initial setup
        mAnimationCoordinator.updateWithoutStacking(m1, false, () -> {});

        // Again with same candidates.
        mAnimationCoordinator.updateWithoutStacking(m1, false, () -> {});

        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m1.handler).show(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testShowMessages_withoutStacking() {
        // Initial values should be null.
        var currentMessage = mAnimationCoordinator.getCurrentDisplayedMessage();
        Assert.assertNull(currentMessage);

        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        mAnimationCoordinator.updateWithoutStacking(m1, false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());

        currentMessage = mAnimationCoordinator.getCurrentDisplayedMessage();
        Assert.assertEquals(m1, currentMessage);
    }

    @Test
    @SmallTest
    public void testHideMessage_withoutStacking() {
        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        mAnimationCoordinator.updateWithoutStacking(m1, false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler, never()).show(Position.INVISIBLE, Position.FRONT);

        mAnimationCoordinator.updateWithoutStacking(m2, false,
                () -> { mAnimationCoordinator.updateWithoutStacking(m2, false, () -> {}); });
        verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        verify(m2.handler).show(Position.INVISIBLE, Position.FRONT);

        var currentMessage = mAnimationCoordinator.getCurrentDisplayedMessage();
        Assert.assertEquals(m2, currentMessage);
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
        callbackHelper.waitForFirst();
        verify(mQueueDelegate).onAnimationEnd();

        // Again with same candidates.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {
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
        HistogramDelta d1 = new HistogramDelta(MessagesMetrics.STACKING_HISTOGRAM_NAME,
                MessagesMetrics.StackingAnimationType.SHOW_ALL);
        HistogramDelta d2 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.INSERT_AT_FRONT),
                1);
        HistogramDelta d3 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.INSERT_AT_BACK),
                2);
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler, never()).hide(anyInt(), anyInt(), anyBoolean());

        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
        Assert.assertEquals(1, d1.getDelta());
        Assert.assertEquals(1, d2.getDelta());
        Assert.assertEquals(1, d3.getDelta());
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

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        HistogramDelta d1 = new HistogramDelta(MessagesMetrics.STACKING_HISTOGRAM_NAME,
                MessagesMetrics.StackingAnimationType.REMOVE_FRONT_AND_SHOW_BACK);
        HistogramDelta d2 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.REMOVE_FRONT),
                1);
        HistogramDelta d3 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.PUSH_TO_FRONT),
                2);
        // Hide the front one so that the back one is brought to front.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, null), false, () -> {});
        inOrder.verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        inOrder.verify(m2.handler).show(Position.BACK, Position.FRONT);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());
        Assert.assertEquals(1, d1.getDelta());
        Assert.assertEquals(1, d2.getDelta());
        Assert.assertEquals(1, d3.getDelta());
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
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        MessageState m3 = buildMessageState();
        setMessageIdentifier(m3, 3);

        // Hide the front one so that the back one is brought to front.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m3), false, () -> {});
        inOrder.verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        inOrder.verify(m2.handler).show(Position.BACK, Position.FRONT);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());

        HistogramDelta d1 = new HistogramDelta(MessagesMetrics.STACKING_HISTOGRAM_NAME,
                MessagesMetrics.StackingAnimationType.SHOW_BACK_ONLY);
        HistogramDelta d2 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.INSERT_AT_BACK),
                3);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m3), false, () -> {});
        verify(m3.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, m3}, currentMessages.toArray());
        Assert.assertEquals(1, d1.getDelta());
        Assert.assertEquals(1, d2.getDelta());
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
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        InOrder inOrder = Mockito.inOrder(m1.handler, m2.handler);
        inOrder.verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m2.handler).show(Position.FRONT, Position.BACK);

        HistogramDelta d1 = new HistogramDelta(MessagesMetrics.STACKING_HISTOGRAM_NAME,
                MessagesMetrics.StackingAnimationType.REMOVE_BACK_ONLY);
        HistogramDelta d2 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.REMOVE_BACK),
                2);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});
        inOrder.verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        inOrder.verify(m2.handler).hide(Position.BACK, Position.FRONT, true);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, null}, currentMessages.toArray());
        Assert.assertEquals(1, d1.getDelta());
        Assert.assertEquals(1, d2.getDelta());
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

        HistogramDelta d1 = new HistogramDelta(MessagesMetrics.STACKING_HISTOGRAM_NAME,
                MessagesMetrics.StackingAnimationType.INSERT_AT_FRONT);
        HistogramDelta d2 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.PUSH_TO_BACK),
                1);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m1), false, () -> {});
        inOrder.verify(m2.handler).show(Position.INVISIBLE, Position.FRONT);
        inOrder.verify(m1.handler).show(Position.FRONT, Position.BACK);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, m1}, currentMessages.toArray());
        Assert.assertEquals(1, d1.getDelta());
        Assert.assertEquals(1, d2.getDelta());
    }

    /**
     * Test messages are hidden before #onStartShow is done.
     */
    @Test
    @SmallTest
    public void testHideBeforeFullyShow() {
        mAnimationCoordinator.setMessageQueueDelegate(new MessageQueueDelegate() {
            @Override
            public void onStartShowing(Runnable callback) {}

            @Override
            public void onFinishHiding() {}

            @Override
            public void onAnimationStart() {}

            @Override
            public void onAnimationEnd() {}
        });
        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, currentMessages.toArray());
        HistogramDelta d1 = new HistogramDelta(MessagesMetrics.STACKING_HISTOGRAM_NAME,
                MessagesMetrics.StackingAnimationType.SHOW_ALL);
        HistogramDelta d2 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.INSERT_AT_FRONT),
                1);
        HistogramDelta d3 = new HistogramDelta(MessagesMetrics.STACKING_ACTION_HISTOGRAM_PREFIX
                        + MessagesMetrics.stackingAnimationActionToHistogramSuffix(
                                MessagesMetrics.StackingAnimationAction.INSERT_AT_BACK),
                2);
        MessageState m1 = buildMessageState();
        setMessageIdentifier(m1, 1);
        MessageState m2 = buildMessageState();
        setMessageIdentifier(m2, 2);
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        Assert.assertEquals(1, d1.getDelta());
        Assert.assertEquals(1, d2.getDelta());
        Assert.assertEquals(1, d3.getDelta());
        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(null, null), false, () -> {});
        verify(m1.handler).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).hide(anyInt(), anyInt(), anyBoolean());
    }

    private void setMessageIdentifier(MessageState message, int messageIdentifier) {
        doReturn(messageIdentifier).when(message.handler).getMessageIdentifier();
    }

    private MessageState buildMessageState() {
        return new MessageState(null, null, Mockito.mock(MessageStateHandler.class), false);
    }
}
