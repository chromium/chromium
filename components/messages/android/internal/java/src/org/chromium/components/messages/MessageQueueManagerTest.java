// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for MessageQueueManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageQueueManagerTest {
    private MessageQueueDelegate mEmptyDelegate = new MessageQueueDelegate() {
        @Override
        public void prepareToShow(Runnable callback) {
            callback.run();
        }

        @Override
        public void prepareToHide(Runnable callback) {
            callback.run();
        }
    };
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
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        MessageStateHandler m2 = Mockito.mock(MessageStateHandler.class);

        queueManager.enqueueMessage(m1, m1);
        verify(m1).show();
        queueManager.dismissMessage(m1);
        verify(m1).hide();
        verify(m1).dismiss();

        queueManager.enqueueMessage(m2, m2);
        verify(m2).show();
        queueManager.dismissMessage(m2);
        verify(m2).hide();
        verify(m2).dismiss();
    }

    /**
     * Tests that, with multiple enqueued messages, only one message is shown at a time.
     */
    @Test
    @SmallTest
    public void testOneMessageShownAtATime() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        MessageStateHandler m2 = Mockito.mock(MessageStateHandler.class);

        queueManager.enqueueMessage(m1, m1);
        queueManager.enqueueMessage(m2, m2);
        verify(m1).show();
        verify(m2, never()).show();

        queueManager.dismissMessage(m1);
        verify(m1).hide();
        verify(m1).dismiss();
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

        queueManager.enqueueMessage(m1, m1);
        queueManager.enqueueMessage(m2, m2);
        verify(m1).show();
        verify(m2, never()).show();

        queueManager.dismissMessage(m2);
        verify(m2).dismiss();

        queueManager.dismissMessage(m1);
        verify(m2, never()).show();
        verify(m2, never()).hide();
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

        queueManager.enqueueMessage(m1, key);
        queueManager.enqueueMessage(m2, key);
    }

    /**
     * Tests that dismissing a message more than once is handled correctly.
     */
    @Test
    @SmallTest
    public void testDismissMessageTwice() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        queueManager.enqueueMessage(m1, m1);
        queueManager.dismissMessage(m1);
        queueManager.dismissMessage(m1);
        verify(m1, times(1)).dismiss();
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
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        queueManager.enqueueMessage(m1, m1);
        verify(delegate, never()).prepareToShow(any());
        verify(delegate, never()).prepareToHide(any());
        verify(m1, never()).show();
        verify(m1, never()).hide();

        queueManager.resume(token);
        verify(delegate).prepareToShow(any());
        verify(m1).show();

        queueManager.suspend();
        verify(delegate).prepareToHide(any());
        verify(m1).hide();
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
        queueManager.enqueueMessage(m1, m1);
        verify(delegate, never()).prepareToShow(any());
        verify(delegate, never()).prepareToHide(any());
        verify(m1, never()).show();
        verify(m1, never()).hide();

        queueManager.dismissMessage(m1);
        verify(delegate, never()).prepareToShow(any());
        verify(delegate, never()).prepareToHide(any());
        verify(m1, never()).show();
        verify(m1, never()).hide();
    }
}
