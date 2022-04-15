// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit tests for MessageQueueManager.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
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

        @Override
        public int getMessageIdentifier() {
            return MessageIdentifier.TEST_MESSAGE;
        }
    }

    private static class ActiveMockWebContents extends MockWebContents {
        @Override
        public ViewAndroidDelegate getViewAndroidDelegate() {
            ViewGroup view = Mockito.mock(ViewGroup.class);
            when(view.getVisibility()).thenReturn(ViewGroup.VISIBLE);
            return ViewAndroidDelegate.createBasicDelegate(view);
        }
    }

    private static class MockWindowAndroidWebContents extends MockWebContents {
        @Override
        public WindowAndroid getTopLevelNativeWindow() {
            // WindowAndroid includes some APIs not available on L. Do not mock this
            // on Android L.
            WindowAndroid windowAndroid = mock(WindowAndroid.class);
            doNothing().when(windowAndroid).addActivityStateObserver(any());
            doReturn(ActivityState.RESUMED).when(windowAndroid).getActivityState();
            return windowAndroid;
        }
    }

    private static final int SCOPE_TYPE = MessageScopeType.NAVIGATION;
    private static final ScopeKey SCOPE_INSTANCE_ID =
            new ScopeKey(SCOPE_TYPE, new ActiveMockWebContents());

    private static final ScopeKey SCOPE_INSTANCE_ID_A =
            new ScopeKey(SCOPE_TYPE, new ActiveMockWebContents());

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
    }

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

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        Assert.assertEquals(1, getEnqueuedMessageCountForTesting(MessageIdentifier.TEST_MESSAGE));
        verify(m1).show();
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1).hide(anyBoolean(), any());
        verify(m1).dismiss(DismissReason.TIMER);
        Assert.assertEquals(
                1, getDismissReasonForTesting(MessageIdentifier.TEST_MESSAGE, DismissReason.TIMER));

        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        Assert.assertEquals(2, getEnqueuedMessageCountForTesting(MessageIdentifier.TEST_MESSAGE));
        verify(m2).show();
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        Assert.assertEquals(
                2, getDismissReasonForTesting(MessageIdentifier.TEST_MESSAGE, DismissReason.TIMER));
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

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m3, m3, SCOPE_INSTANCE_ID_A, false);

        queueManager.dismissAllMessages(DismissReason.ACTIVITY_DESTROYED);
        Assert.assertEquals(3,
                getDismissReasonForTesting(
                        MessageIdentifier.TEST_MESSAGE, DismissReason.ACTIVITY_DESTROYED));
        verify(m1).dismiss(DismissReason.ACTIVITY_DESTROYED);
        verify(m2).dismiss(DismissReason.ACTIVITY_DESTROYED);
        verify(m3).dismiss(DismissReason.ACTIVITY_DESTROYED);

        Assert.assertTrue("#dismissAllMessages should clear the message queue.",
                queueManager.getMessagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testMessageShouldShow() {
        MessageQueueManager queueManager = new MessageQueueManager();

        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        when(m1.shouldShow())
                .thenReturn(true, true,
                        true, // This is called by #getNextMessage after a message is shown.
                        true, false);

        // m1#shouldShow will be invoked and will return true.
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);

        // The enqueued message should be chosen as a candidate to be shown.
        // m1#shouldShow will be invoked and will return true.
        MessageState messageState = queueManager.getNextMessage();
        Assert.assertNotNull("Next message candidate should not be null.", messageState);

        // The enqueued message should not be chosen as a candidate to be shown.
        // m1#shouldShow will be invoked and will return false.
        messageState = queueManager.getNextMessage();
        Assert.assertNull("Next message candidate should be null.", messageState);

        verify(m1, times(5)).shouldShow();

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1).hide(anyBoolean(), any());
        verify(m1).dismiss(DismissReason.TIMER);
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

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
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
        when(m1.shouldShow()).thenReturn(true);
        when(m2.shouldShow()).thenReturn(true);

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
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

        queueManager.enqueueMessage(m1, key, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, key, SCOPE_INSTANCE_ID, false);
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
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
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
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
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
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
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
        final ScopeKey inactiveScopeKey = new ScopeKey(SCOPE_TYPE, new MockWebContents());
        final ScopeKey inactiveScopeKey2 = new ScopeKey(SCOPE_TYPE, new MockWebContents());
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, inactiveScopeKey2, false);

        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m2, m2, inactiveScopeKey, false);

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
     * Test that messages of multiple scope types can be correctly shown.
     */
    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M)
    public void testMessageOnMultipleScopeTypes() {
        // DO not mock WindowAndroid on L
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        final ScopeKey navScopeKey =
                new ScopeKey(MessageScopeType.NAVIGATION, new ActiveMockWebContents());
        final ScopeKey windowScopeKey =
                new ScopeKey(new MockWindowAndroidWebContents().getTopLevelNativeWindow());

        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, navScopeKey, false);

        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m2, m2, windowScopeKey, false);

        verify(m1, description("A message should be shown when the associated scope is active"))
                .show();
        verify(m1,
                never().description(
                        "The message should not be hidden when its scope is still active"))
                .hide(anyBoolean(), any());

        verify(m2,
                never().description("The message should not be visible when its scope is inactive"))
                .show();

        queueManager.onScopeChange(new MessageScopeChange(
                MessageScopeType.NAVIGATION, navScopeKey, ChangeType.DESTROY));

        verify(m1, description("The message should be hidden when its scope is inactive"))
                .hide(anyBoolean(), any());

        verify(m1, description("The message should be dismissed when its scope is destroyed"))
                .dismiss(anyInt());

        verify(m2, description("A message should be shown when the associated scope is active"))
                .show();

        queueManager.onScopeChange(new MessageScopeChange(
                MessageScopeType.WINDOW, windowScopeKey, ChangeType.DESTROY));

        verify(m2, description("The message should be hidden when its scope is inactive"))
                .hide(anyBoolean(), any());

        verify(m2, description("The message should be dismissed when its scope is destroyed"))
                .dismiss(anyInt());
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
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));

        verify(m1,
                description(
                        "A message should be hidden with animation when animationTransition is set true."))
                .hide(eq(true), any());
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

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
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
     * Test that callback can be correctly called if #hide is called without #show called before.
     */
    @Test
    @SmallTest
    public void testShowHideMultipleTimes() {
        MessageQueueDelegate delegate = Mockito.spy(MessageQueueDelegate.class);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);

        // Show and hide twice.
        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(delegate).onStartShowing(runnableCaptor.capture());
        Runnable onShow = runnableCaptor.getValue();
        verify(m1, never()).show();
        // Become inactive before onStartShowing is finished.
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));
        verify(m1, never()).hide(anyBoolean(), any());
        onShow.run();
        verify(m1, never()).show();

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(delegate, times(2)).onStartShowing(runnableCaptor.capture());
        runnableCaptor.getValue().run();
        verify(m1).show();

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.DESTROY));
        verify(m1).hide(anyBoolean(), any());
    }

    @Test
    @SmallTest
    public void testSuspendDuringOnStartingShow() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        verify(m1).show();

        // Hide
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));
        verify(m1).hide(anyBoolean(), any());

        // Re-assign a delegate so that onStartShowing will not trigger callback at once.
        delegate = Mockito.spy(MessageQueueDelegate.class);
        queueManager.setDelegate(delegate);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        // Suspend queue before onStartShowing is finished.
        queueManager.suspend();

        verify(m1,
                times(1).description(
                        "Message should not show again before onStartShowing is finished"))
                .show();
        verify(m1, times(1).description("Message should not call #hide if it is not shown before"))
                .hide(anyBoolean(), any());
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

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(controller,
                description(
                        "ScopeChangeController should be notified when the queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);

        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        verify(controller,
                times(1).description(
                        "ScopeChangeController should be notified **only** when the queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);

        queueManager.enqueueMessage(m3, m3, SCOPE_INSTANCE_ID_A, false);
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

    /**
     * Test that the higher priority message is displayed when being enqueued.
     */
    @Test
    @SmallTest
    public void testEnqueueHigherPriorityMessage() {
        MessageQueueManager queueManager = new MessageQueueManager();
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(m1).show();

        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, true);
        verify(m1).hide(anyBoolean(), any());
        verify(m2).show();
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        verify(m2).hide(anyBoolean(), any());
        verify(m2).dismiss(DismissReason.TIMER);
        verify(m1, times(2)).show();
    }

    static int getEnqueuedMessageCountForTesting(@MessageIdentifier int messageIdentifier) {
        return RecordHistogram.getHistogramValueCountForTesting(
                MessagesMetrics.getEnqueuedHistogramNameForTesting(), messageIdentifier);
    }

    static int getDismissReasonForTesting(
            @MessageIdentifier int messageIdentifier, @DismissReason int dismissReason) {
        String histogramName = MessagesMetrics.getDismissHistogramNameForTesting(messageIdentifier);
        return RecordHistogram.getHistogramValueCountForTesting(histogramName, dismissReason);
    }
}
