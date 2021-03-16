// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.test.mock.MockWebContents;

/**
 * Unit tests for MessageQueueManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageQueueManagerTest {
    private MessageQueueDelegate mEmptyDelegate = new MessageQueueDelegate() {
        @Override
        public void onStartShowing(Runnable callback) {
            callback.run();
        }

        @Override
        public void onFinishHiding() {}
    };

    private class EmptyMessageStateHandler implements MessageStateHandler {
        @Override
        public void show() {}

        @Override
        public void hide(boolean animate, Runnable hiddenCallback) {
            hiddenCallback.run();
        }

        @Override
        public void dismiss(@DismissReason int dismissReason) {}
    }

    private static class InactiveMockWebContents extends MockWebContents {
        @Override
        public @Visibility int getVisibility() {
            return Visibility.HIDDEN;
        }
    }

    private static final int SCOPE_TYPE = 0;
    private static final ScopeKey SCOPE_INSTANCE_ID =
            new ScopeKey(SCOPE_TYPE, new MockWebContents());

    private static final ScopeKey SCOPE_INSTANCE_ID_A =
            new ScopeKey(SCOPE_TYPE, new MockWebContents());

    /**
     * Tests lifecycle of a single message:
     *   - enqueueMessage() calls show()
     *   - dismissMessage() calls hide() and dismiss()
     */
    @Test
    @SmallTest
    public void testEnqueueMessage() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(m1).show();
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1).hide(anyBoolean(), any());
        verify(m1).dismiss(DismissReason.TIMER);

        queueManager.enqueueMessage(m2, m2, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(m2).show();
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        verify(m2).hide(anyBoolean(), any());
        verify(m2).dismiss(DismissReason.TIMER);
    }

    /**
     * Test method {@link MessageQueueManager#dismissAllMessages(int)}.
     */
    @Test
    @SmallTest
    public void testDismissAllMessages() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m3 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.enqueueMessage(m2, m2, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.enqueueMessage(m3, m3, SCOPE_TYPE, SCOPE_INSTANCE_ID_A);

        queueManager.dismissAllMessages(DismissReason.ACTIVITY_DESTROYED);
        verify(m1).dismiss(DismissReason.ACTIVITY_DESTROYED);
        verify(m2).dismiss(DismissReason.ACTIVITY_DESTROYED);
        verify(m3).dismiss(DismissReason.ACTIVITY_DESTROYED);

        Assert.assertTrue("#dismissAllMessages should clear the message queue.",
                queueManager.getMessagesForTesting().isEmpty());
    }

    /**
     * Tests that, with multiple enqueued messages, only one message is shown at a time.
     */
    @Test
    @SmallTest
    public void testOneMessageShownAtATime() {
        MessageQueueManager queueManager = new MessageQueueManager();

        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.enqueueMessage(m2, m2, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        verify(m1).show();
        verify(m2, never()).show();

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1).hide(anyBoolean(), any());
        verify(m1).dismiss(DismissReason.TIMER);
        verify(m2).show();
    }

    /**
     * Tests that, when the message is dismissed before it was shown, neither show() nor hide() is
     * called.
     */
    @Test
    @SmallTest
    public void testDismissBeforeShow() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        MessageStateHandler m2 = Mockito.mock(MessageStateHandler.class);

        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.enqueueMessage(m2, m2, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(m1).show();
        verify(m2, never()).show();

        queueManager.dismissMessage(m2, DismissReason.TIMER);
        verify(m2).dismiss(DismissReason.TIMER);

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m2, never()).show();
        verify(m2, never()).hide(anyBoolean(), any());
    }

    /**
     * Tests that enqueueing two messages with the same key is not allowed, it results in
     * IllegalStateException.
     */
    @Test(expected = IllegalStateException.class)
    @SmallTest
    public void testEnqueueDuplicateKey() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        MessageStateHandler m2 = Mockito.mock(MessageStateHandler.class);
        Object key = new Object();

        queueManager.enqueueMessage(m1, key, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.enqueueMessage(m2, key, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
    }

    /**
     * Tests that dismissing a message more than once is handled correctly.
     */
    @Test
    @SmallTest
    public void testDismissMessageTwice() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1, times(1)).dismiss(DismissReason.TIMER);
    }

    /**
     * Tests that delegate methods are properly called when queue is suspended
     * and resumed.
     */
    @Test
    @SmallTest
    public void testSuspendAndResumeQueue() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        int token = queueManager.suspend();
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(delegate, never()).onStartShowing(any());
        verify(delegate, never()).onFinishHiding();
        verify(m1, never()).show();
        verify(m1, never()).hide(anyBoolean(), any());

        queueManager.resume(token);
        verify(delegate).onStartShowing(any());
        verify(m1).show();

        queueManager.suspend();
        verify(delegate).onFinishHiding();
        verify(m1).hide(anyBoolean(), any());
    }

    /**
     * Tests that delegate methods are properly called to show/hide message
     * when queue is suspended.
     */
    @Test
    @SmallTest
    public void testDismissOnSuspend() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        queueManager.suspend();
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(delegate, never()).onStartShowing(any());
        verify(delegate, never()).onFinishHiding();
        verify(m1, never()).show();
        verify(m1, never()).hide(anyBoolean(), any());

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(delegate, never()).onStartShowing(any());
        verify(delegate, never()).onFinishHiding();
        verify(m1, never()).show();
        verify(m1, never()).hide(anyBoolean(), any());
    }

    /**
     * Test that the message can show/hide correctly when its corresponding scope is
     * activated/deactivated.
     */
    @Test
    @SmallTest
    public void testMessageShowOnScopeChange() {
        // TODO(crbug.com/1163290): cover more various scenarios, such as re-activating scopes
        //                          which have been destroyed.
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        final ScopeKey inactiveScopeKey = new ScopeKey(SCOPE_TYPE, new InactiveMockWebContents());
        final ScopeKey inactiveScopeKey2 = new ScopeKey(SCOPE_TYPE, new InactiveMockWebContents());
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, inactiveScopeKey2);

        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m2, m2, SCOPE_TYPE, inactiveScopeKey);

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, inactiveScopeKey, ChangeType.ACTIVE));
        verify(m1, never().description("A message should not be shown on another scope instance."))
                .show();
        verify(m1,
                never().description(
                        "A message should never have been shown or hidden before its target scope is activated."))
                .hide(anyBoolean(), any());

        verify(m2, description("A message should show on its target scope instance.")).show();

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, inactiveScopeKey, ChangeType.INACTIVE));
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, inactiveScopeKey2, ChangeType.ACTIVE));

        verify(m2,
                description(
                        "The message should hide when its target scope instance is deactivated."))
                .hide(anyBoolean(), any());
        verify(m1,
                description("The message should show when its target scope instance is activated."))
                .show();
        verify(m1,
                never().description(
                        "The message should stay on the screen when its target scope instance is activated."))
                .hide(anyBoolean(), any());
    }

    /**
     * Test that animateTransition gets propagated from MessageScopeChange to hide() call correctly.
     */
    @Test
    @SmallTest
    public void testMessageAnimationTransitionOnScopeChange() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE, true));

        verify(m1,
                description(
                        "A message should be hidden with animation when animationTransition is set true."))
                .hide(eq(true), any());

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE, false));

        verify(m1,
                description(
                        "A message should be hidden without animation when animationTransition is set false."))
                .hide(eq(false), any());
    }

    /**
     * Test that the message is correctly dismissed when the scope is destroyed.
     */
    @Test
    @SmallTest
    public void testMessageDismissedOnScopeDestroy() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(m1,
                description("The message should show when its target scope instance is activated."))
                .show();

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.DESTROY));
        verify(m1,
                description("The message should hide when its target scope instance is destroyed."))
                .hide(anyBoolean(), any());
        verify(m1,
                description(
                        "Message should be dismissed when its target scope instance is destroyed."))
                .dismiss(anyInt());
    }

    /**
     * Test scope change controller is properly called when message is enqueued and dismissed.
     */
    @Test
    @SmallTest
    public void testScopeChangeControllerInvoked() {
        ScopeChangeController controller = Mockito.mock(ScopeChangeController.class);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setScopeChangeControllerForTesting(controller);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m3 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(controller,
                description(
                        "ScopeChangeController should be notified when the queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);

        queueManager.enqueueMessage(m2, m2, SCOPE_TYPE, SCOPE_INSTANCE_ID);
        verify(controller,
                times(1).description(
                        "ScopeChangeController should be notified **only** when the queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);

        queueManager.enqueueMessage(m3, m3, SCOPE_TYPE, SCOPE_INSTANCE_ID_A);
        verify(controller,
                times(1).description(
                        "ScopeChangeController should be notified **only** when the queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);
        verify(controller,
                description(
                        "ScopeChangeController should be notified when the queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID_A);

        queueManager.dismissMessage(m3, DismissReason.TIMER);
        verify(controller,
                never().description(
                        "ScopeChangeController should not be notified when the queue of scope is not empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID);
        verify(controller,
                description(
                        "ScopeChangeController should be notified when the queue of scope becomes empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID_A);

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(controller,
                never().description(
                        "ScopeChangeController should not be notified when the queue of scope is not empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID);
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        verify(controller,
                description(
                        "ScopeChangeController should be notified when the queue of scope is empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID);
    }
}
