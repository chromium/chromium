// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.animation.Animator;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageStateHandler.Position;

import java.util.Arrays;

/**
 * Unit tests for {@link MessageAnimationCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageAnimationCoordinatorUnitTest {
    private MessageQueueDelegate mQueueDelegate = new MessageQueueDelegate() {
        @Override
        public void onStartShowing(Runnable callback) {
            callback.run();
        }

        @Override
        public void onFinishHiding() {}
    };

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MessageContainer mContainer;

    @Mock
    private Callback<Animator> mAnimatorStartCallback;

    private MessageAnimationCoordinator mAnimationCoordinator;

    @Before
    public void setUp() {
        mAnimationCoordinator = new MessageAnimationCoordinator(mContainer, mAnimatorStartCallback);
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
    public void testDoNothing() {
        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        // Initial setup
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        // Again with same candidates.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

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

        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);
        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler, never()).hide(anyInt(), anyInt(), anyBoolean());

        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, m2}, currentMessages.toArray());
    }

    // Test only front message becomes hidden.
    // [m1, m2] -> [m2, null]
    @Test
    @SmallTest
    public void testHideFrontMessageOnly() {
        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);

        // Hide the front one so that the back one is brought to front.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, null), false, () -> {});
        verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        verify(m2.handler).show(Position.BACK, Position.FRONT);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());
    }

    // Test hiding front one and then showing a new one.
    // [m1, m2] -> [m2, m3] is done in two steps.
    // [m1, m2] -> [m2, null], then [m2, null] -> [m2, m3]
    @Test
    @SmallTest
    public void testDismissFrontAndEnqueueNew() {
        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);

        MessageState m3 = buildMessageState();
        // Hide the front one so that the back one is brought to front.
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m3), false, () -> {});
        verify(m1.handler).hide(Position.FRONT, Position.INVISIBLE, true);
        verify(m2.handler).show(Position.BACK, Position.FRONT);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, null}, currentMessages.toArray());

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m2, m3), false, () -> {});
        verify(m3.handler).show(Position.FRONT, Position.BACK);
        currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m2, m3}, currentMessages.toArray());
    }

    // Test only back message becomes hidden.
    // [m1, m2] -> [m1, null]
    @Test
    @SmallTest
    public void testHiddenBackMessageOnly() {
        MessageState m1 = buildMessageState();
        MessageState m2 = buildMessageState();
        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, m2), false, () -> {});

        verify(m1.handler).show(Position.INVISIBLE, Position.FRONT);
        verify(m2.handler).show(Position.FRONT, Position.BACK);

        mAnimationCoordinator.updateWithStacking(Arrays.asList(m1, null), false, () -> {});
        verify(m1.handler, never()).hide(anyInt(), anyInt(), anyBoolean());
        verify(m2.handler).hide(Position.BACK, Position.FRONT, true);

        var currentMessages = mAnimationCoordinator.getCurrentDisplayedMessages();
        Assert.assertArrayEquals(new MessageState[] {m1, null}, currentMessages.toArray());
    }

    private MessageState buildMessageState() {
        return new MessageState(null, null, Mockito.mock(MessageStateHandler.class), false);
    }
}
