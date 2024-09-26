// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.animation.AnimatorSet;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.messages.MessageQueueManager.MessageState;
import org.chromium.components.messages.MessageScopeChange.ChangeType;
import org.chromium.components.messages.MessageStateHandler.Position;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for MessageQueueManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({MessageFeatureList.MESSAGES_ANDROID_EXTRA_HISTOGRAMS})
public class MessageQueueManagerTest {

    private MessageQueueDelegate mEmptyDelegate =
            new MessageQueueDelegate() {
                boolean mIsReadyForShowing;

                @Override
                public void onRequestShowing(Runnable callback) {
                    mIsReadyForShowing = true;
                    callback.run();
                }

                @Override
                public void onFinishHiding() {
                    mIsReadyForShowing = false;
                }

                @Override
                public void onAnimationStart() {}

                @Override
                public void onAnimationEnd() {}

                @Override
                public boolean isReadyForShowing() {
                    return mIsReadyForShowing;
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
            };

    private static class EmptyMessageStateHandler implements MessageStateHandler {

        private int mId = MessageIdentifier.TEST_MESSAGE;

        public EmptyMessageStateHandler(int id) {
            mId = id;
        }

        public EmptyMessageStateHandler() {}

        @Override
        public Animator show(int fromIndex, int toIndex) {
            return new AnimatorSet();
        }

        @Override
        public Animator hide(int fromIndex, int toIndex, boolean animate) {
            return new AnimatorSet();
        }

        @Override
        public void dismiss(@DismissReason int dismissReason) {}

        @Override
        public int getMessageIdentifier() {
            return mId;
        }
    }

    private static class ActiveMockWebContents extends MockWebContents {
        @Override
        public @Visibility int getVisibility() {
            return Visibility.VISIBLE;
        }
    }

    private static class InactiveMockWebContents extends MockWebContents {
        @Override
        public @Visibility int getVisibility() {
            return Visibility.HIDDEN;
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

    private MessageAnimationCoordinator mAnimationCoordinator;

    @Before
    public void setUp() {
        MessageContainer container = Mockito.mock(MessageContainer.class);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(container)
                .runAfterInitialMessageLayout(any(Runnable.class));
        doReturn(false).when(container).isIsInitializingLayout();
        mAnimationCoordinator = new MessageAnimationCoordinator(container, Animator::start);
    }

    /**
     * Tests lifecycle of a single message: - enqueueMessage() calls show() - dismissMessage() calls
     * hide() and dismiss()
     */
    @Test
    @SmallTest
    public void testEnqueueMessage() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.POPUP_BLOCKED));
        MessageStateHandler m2 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.SYNC_ERROR));

        var enqueued =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.Messages.Enqueued", m1.getMessageIdentifier())
                        .expectIntRecord(
                                "Android.Messages.Enqueued.Visible", m1.getMessageIdentifier())
                        .expectNoRecords("Android.Messages.Enqueued.Hiding")
                        .expectNoRecords("Android.Messages.Enqueued.Hidden")
                        .build();
        var dismissed =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Dismissed.PopupBlocked", DismissReason.TIMER);
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        enqueued.assertExpected();

        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(m1).dismiss(DismissReason.TIMER);
        dismissed.assertExpected();

        enqueued =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Enqueued", m2.getMessageIdentifier());
        dismissed =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Dismissed.SyncError", DismissReason.TIMER);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        enqueued.assertExpected();
        verify(m2).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        dismissed.assertExpected();
        verify(m2).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(m2).dismiss(DismissReason.TIMER);
    }

    /**
     * Tests lifecycle of a single message: - enqueueMessage() calls show() - dismissMessage() calls
     * hide() and dismiss() when a queue is enqueued with multiple messages
     */
    @Test
    @SmallTest
    public void testEnqueueMultipleMessages() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.POPUP_BLOCKED));
        MessageStateHandler m2 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.SYNC_ERROR));

        var enqueued =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Messages.Enqueued",
                                m1.getMessageIdentifier(),
                                m2.getMessageIdentifier())
                        .expectIntRecord(
                                "Android.Messages.Enqueued.Visible", m1.getMessageIdentifier())
                        .expectIntRecord(
                                "Android.Messages.Enqueued.Hiding", m1.getMessageIdentifier())
                        .expectIntRecord(
                                "Android.Messages.Enqueued.Hidden", m2.getMessageIdentifier())
                        .build();
        var dismissed =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Dismissed.PopupBlocked", DismissReason.TIMER);
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        enqueued.assertExpected();

        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(m1).dismiss(DismissReason.TIMER);
        dismissed.assertExpected();

        dismissed =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Messages.Dismissed.SyncError", DismissReason.TIMER);
        enqueued.assertExpected();
        verify(m2).show(eq(Position.BACK), eq(Position.FRONT));
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        dismissed.assertExpected();
        verify(m2).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(m2).dismiss(DismissReason.TIMER);
    }

    /** Histograms are recorded with whether queue is suspended. */
    @Test
    @SmallTest
    public void testEnqueueWithQueueSuspension() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        int token = queueManager.suspend();

        MessageStateHandler m1 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.POPUP_BLOCKED));
        MessageStateHandler m2 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.SYNC_ERROR));

        var enqueued =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Messages.Enqueued",
                                m1.getMessageIdentifier(),
                                m2.getMessageIdentifier())
                        .expectIntRecords(
                                "Android.Messages.Enqueued.Suspended",
                                m1.getMessageIdentifier(),
                                m2.getMessageIdentifier())
                        .expectNoRecords("Android.Messages.Enqueued.Resumed")
                        .build();

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        enqueued.assertExpected();

        MessageStateHandler m3 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.ABOUT_THIS_SITE));
        MessageStateHandler m4 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.DOWNLOAD_PROGRESS));
        enqueued =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Messages.Enqueued",
                                m3.getMessageIdentifier(),
                                m4.getMessageIdentifier())
                        .expectIntRecords(
                                "Android.Messages.Enqueued.Resumed",
                                m3.getMessageIdentifier(),
                                m4.getMessageIdentifier())
                        .expectNoRecords("Android.Messages.Enqueued.Suspended")
                        .build();
        queueManager.resume(token);

        queueManager.enqueueMessage(m3, m3, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m4, m4, SCOPE_INSTANCE_ID, false);
        enqueued.assertExpected();
    }

    /** Histograms are recorded with whether scope is active. */
    @Test
    @SmallTest
    public void testEnqueueWithScopeActivation() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);

        MessageStateHandler m1 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.POPUP_BLOCKED));
        MessageStateHandler m2 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.SYNC_ERROR));
        MessageStateHandler m3 =
                Mockito.spy(new EmptyMessageStateHandler(MessageIdentifier.DOWNLOAD_PROGRESS));

        var enqueued =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.Messages.Enqueued",
                                m1.getMessageIdentifier(),
                                m2.getMessageIdentifier(),
                                m3.getMessageIdentifier())
                        .expectIntRecords(
                                "Android.Messages.Enqueued.ScopeInactive",
                                m2.getMessageIdentifier())
                        .expectIntRecords(
                                "Android.Messages.Enqueued.ScopeActive",
                                m1.getMessageIdentifier(),
                                m3.getMessageIdentifier())
                        .build();

        final ScopeKey inactiveScopeKey = new ScopeKey(SCOPE_TYPE, new InactiveMockWebContents());
        final ScopeKey windowScopeKey =
                new ScopeKey(new MockWindowAndroidWebContents().getTopLevelNativeWindow());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, inactiveScopeKey, true);
        queueManager.enqueueMessage(m3, m3, windowScopeKey, false);
        enqueued.assertExpected();

        // Do not record again when there scopes are updated
        enqueued =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Messages.Enqueued.ScopeInactive")
                        .expectNoRecords("Android.Messages.Enqueued.ScopeActive")
                        .build();
        queueManager.onScopeChange(
                new MessageScopeChange(
                        MessageScopeType.NAVIGATION, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));
        queueManager.onScopeChange(
                new MessageScopeChange(
                        MessageScopeType.NAVIGATION, inactiveScopeKey, ChangeType.ACTIVE));
        enqueued.assertExpected();
    }

    /** Test method {@link MessageQueueManager#dismissAllMessages(int)}. */
    @Test
    @SmallTest
    public void testDismissAllMessages() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m3 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m3, m3, SCOPE_INSTANCE_ID_A, false);

        var dismissed =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.Messages.Dismissed.TestMessage",
                                DismissReason.ACTIVITY_DESTROYED,
                                3)
                        .build();
        queueManager.dismissAllMessages(DismissReason.ACTIVITY_DESTROYED);
        dismissed.assertExpected();
        verify(m1).dismiss(DismissReason.ACTIVITY_DESTROYED);
        verify(m2).dismiss(DismissReason.ACTIVITY_DESTROYED);
        verify(m3).dismiss(DismissReason.ACTIVITY_DESTROYED);

        Assert.assertTrue(
                "#dismissAllMessages should clear the message queue.",
                queueManager.getMessagesForTesting().isEmpty());
    }

    /**
     * Tests that, when the message is dismissed before it was shown, neither show() nor hide() is
     * called.
     */
    @Test
    @SmallTest
    public void testDismissBeforeShow() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        MessageStateHandler m2 = Mockito.mock(MessageStateHandler.class);

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(m2, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.dismissMessage(m2, DismissReason.TIMER);

        verify(m2).dismiss(DismissReason.TIMER);
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        Assert.assertNull(queueManager.getNextMessage());
        verify(m2, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(m2, never()).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
    }

    /**
     * Tests that enqueueing two messages with the same key is not allowed, it results in
     * IllegalStateException.
     */
    @Test(expected = IllegalStateException.class)
    @SmallTest
    public void testEnqueueDuplicateKey() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        MessageStateHandler m2 = Mockito.mock(MessageStateHandler.class);
        Object key = new Object();

        queueManager.enqueueMessage(m1, key, SCOPE_INSTANCE_ID, false);
        queueManager.enqueueMessage(m2, key, SCOPE_INSTANCE_ID, false);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
    }

    /** Tests that dismissing a message more than once is handled correctly. */
    @Test
    @SmallTest
    public void testDismissMessageTwice() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(m1, times(1)).dismiss(DismissReason.TIMER);
    }

    /** Tests that delegate methods are properly called when queue is suspended and resumed. */
    @Test
    @SmallTest
    public void testSuspendAndResumeQueue() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        int token = queueManager.suspend();
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(delegate, never()).onRequestShowing(any());
        verify(delegate, never()).onFinishHiding();
        verify(m1, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(m1, never()).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        queueManager.resume(token);
        verify(delegate).onRequestShowing(any());
        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.suspend();
        verify(delegate).onFinishHiding();
        verify(m1).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
    }

    /** Tests that delegate methods are properly called to show/hide message when queue is suspended. */
    @Test
    @SmallTest
    public void testDismissOnSuspend() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        queueManager.suspend();
        MessageStateHandler m1 = Mockito.mock(MessageStateHandler.class);
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(delegate, never()).onRequestShowing(any());
        verify(delegate, never()).onFinishHiding();
        verify(m1, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(m1, never()).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(delegate, never()).onRequestShowing(any());
        verify(delegate, never()).onFinishHiding();
        verify(m1, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(m1, never()).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
    }

    /**
     * Test that the message can show/hide correctly when its corresponding scope is
     * activated/deactivated.
     */
    @Test
    @SmallTest
    public void testMessageShowOnScopeChange() {
        // TODO(crbug.com/40740060): cover more various scenarios, such as re-activating scopes
        //                          which have been destroyed.
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        final ScopeKey inactiveScopeKey = new ScopeKey(SCOPE_TYPE, new InactiveMockWebContents());
        final ScopeKey inactiveScopeKey2 = new ScopeKey(SCOPE_TYPE, new InactiveMockWebContents());
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, inactiveScopeKey2, false);

        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m2, m2, inactiveScopeKey, false);

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, inactiveScopeKey, ChangeType.ACTIVE));
        verify(m1, never().description("A message should not be shown on another scope instance."))
                .show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(
                        m1,
                        never().description(
                                        "A message should never have been shown or hidden before"
                                                + " its target scope is activated."))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        verify(m2, description("A message should show on its target scope instance."))
                .show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, inactiveScopeKey, ChangeType.INACTIVE));
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, inactiveScopeKey2, ChangeType.ACTIVE));

        verify(
                        m2,
                        description(
                                "The message should hide when its target scope instance is"
                                        + " deactivated."))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(
                        m1,
                        description(
                                "The message should show when its target scope instance is"
                                        + " activated."))
                .show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(
                        m1,
                        never().description(
                                        "The message should stay on the screen when its target"
                                                + " scope instance is activated."))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
    }

    /** Test that messages of multiple scope types can be correctly shown. */
    @Test
    @SmallTest
    public void testMessageOnMultipleScopeTypes() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
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
                .show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(
                        m1,
                        never().description(
                                        "The message should not be hidden when its scope is still"
                                                + " active"))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        verify(m2, description("The message should show in the background"))
                .show(eq(Position.FRONT), eq(Position.BACK));

        queueManager.onScopeChange(
                new MessageScopeChange(
                        MessageScopeType.NAVIGATION, navScopeKey, ChangeType.DESTROY));

        verify(m1, description("The message should be hidden when its scope is inactive"))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        verify(m1, description("The message should be dismissed when its scope is destroyed"))
                .dismiss(anyInt());

        verify(m2, description("A message should be shown when the associated scope is active"))
                .show(eq(Position.BACK), eq(Position.FRONT));

        queueManager.onScopeChange(
                new MessageScopeChange(
                        MessageScopeType.WINDOW, windowScopeKey, ChangeType.DESTROY));

        verify(m2, description("The message should be hidden when its scope is inactive"))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        verify(m2, description("The message should be dismissed when its scope is destroyed"))
                .dismiss(anyInt());
    }

    /** Test that animateTransition gets propagated from MessageScopeChange to hide() call correctly. */
    @Test
    @SmallTest
    public void testMessageAnimationTransitionOnScopeChange() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));

        verify(
                        m1,
                        description(
                                "A message should be hidden with animation when animationTransition"
                                        + " is set true."))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), eq(true));
    }

    /** Test that the message is correctly dismissed when the scope is destroyed. */
    @Test
    @SmallTest
    public void testMessageDismissedOnScopeDestroy() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(
                        m1,
                        description(
                                "The message should show when its target scope instance is"
                                        + " activated."))
                .show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.DESTROY));
        verify(
                        m1,
                        description(
                                "The message should hide when its target scope instance is"
                                        + " destroyed."))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(
                        m1,
                        description(
                                "Message should be dismissed when its target scope instance is"
                                        + " destroyed."))
                .dismiss(anyInt());
    }

    /** Test that callback can be correctly called if #hide is called without #show called before. */
    @Test
    @SmallTest
    public void testShowHideMultipleTimes() {
        MessageQueueDelegate delegate = Mockito.spy(MessageQueueDelegate.class);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);

        // Show and hide twice.
        ArgumentCaptor<Runnable> runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(delegate).onRequestShowing(runnableCaptor.capture());
        Runnable onShow = runnableCaptor.getValue();
        verify(m1, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        // Become inactive before onStartShowing is finished.
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));
        verify(m1, never()).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        onShow.run();
        verify(m1, never()).show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        runnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(delegate, times(2)).onRequestShowing(runnableCaptor.capture());
        doReturn(true).when(delegate).isReadyForShowing();
        runnableCaptor.getValue().run();
        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.DESTROY));
        verify(m1).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
    }

    @Test
    @SmallTest
    public void testSuspendDuringOnStartingShow() {
        MessageQueueDelegate delegate = Mockito.spy(mEmptyDelegate);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(delegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);

        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));

        // Hide
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.INACTIVE));
        verify(m1).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());

        // Re-assign a delegate so that onStartShowing will not trigger callback at once.
        delegate = Mockito.spy(MessageQueueDelegate.class);
        queueManager.setDelegate(delegate);
        queueManager.onScopeChange(
                new MessageScopeChange(SCOPE_TYPE, SCOPE_INSTANCE_ID, ChangeType.ACTIVE));
        // Suspend queue before onStartShowing is finished.
        queueManager.suspend();

        verify(
                        m1,
                        times(1).description(
                                        "Message should not show again before onStartShowing is"
                                                + " finished"))
                .show(eq(Position.INVISIBLE), eq(Position.FRONT));
        verify(m1, times(1).description("Message should not call #hide if it is not shown before"))
                .hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
    }

    /** Test scope change controller is properly called when message is enqueued and dismissed. */
    @Test
    @SmallTest
    public void testScopeChangeControllerInvoked() {
        ScopeChangeController controller = Mockito.mock(ScopeChangeController.class);
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        queueManager.setScopeChangeControllerForTesting(controller);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m3 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(
                        controller,
                        description(
                                "ScopeChangeController should be notified when the queue of scope"
                                        + " gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);

        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        verify(
                        controller,
                        times(1).description(
                                        "ScopeChangeController should be notified **only** when the"
                                                + " queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);

        queueManager.enqueueMessage(m3, m3, SCOPE_INSTANCE_ID_A, false);
        verify(
                        controller,
                        times(1).description(
                                        "ScopeChangeController should be notified **only** when the"
                                                + " queue of scope gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID);
        verify(
                        controller,
                        description(
                                "ScopeChangeController should be notified when the queue of scope"
                                        + " gets its first message"))
                .firstMessageEnqueued(SCOPE_INSTANCE_ID_A);

        queueManager.dismissMessage(m3, DismissReason.TIMER);
        verify(
                        controller,
                        never().description(
                                        "ScopeChangeController should not be notified when the"
                                                + " queue of scope is not empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID);
        verify(
                        controller,
                        description(
                                "ScopeChangeController should be notified when the queue of scope"
                                        + " becomes empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID_A);

        queueManager.dismissMessage(m1, DismissReason.TIMER);
        verify(
                        controller,
                        never().description(
                                        "ScopeChangeController should not be notified when the"
                                                + " queue of scope is not empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID);
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        verify(
                        controller,
                        description(
                                "ScopeChangeController should be notified when the queue of scope"
                                        + " is empty."))
                .lastMessageDismissed(SCOPE_INSTANCE_ID);
    }

    /** Test that the higher priority message is displayed when being enqueued. */
    @Test
    @SmallTest
    public void testEnqueueHigherPriorityMessage() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        verify(m1).show(eq(Position.INVISIBLE), eq(Position.FRONT));

        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, true);
        verify(m1).show(eq(Position.FRONT), eq(Position.BACK));
        verify(m2).show(eq(Position.INVISIBLE), eq(Position.FRONT));
        queueManager.dismissMessage(m2, DismissReason.TIMER);
        verify(m2).hide(eq(Position.FRONT), eq(Position.INVISIBLE), anyBoolean());
        verify(m2).dismiss(DismissReason.TIMER);
        verify(m1).show(eq(Position.BACK), eq(Position.FRONT));
    }

    /** Test that {@link MessageQueueManager#getNextMessages()} returns correct list. */
    @Test
    @SmallTest
    public void testGetNextTwoMessages() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        queueManager.setDelegate(mEmptyDelegate);
        var messages = queueManager.getNextMessages();
        Assert.assertArrayEquals(new MessageState[] {null, null}, messages.toArray());
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        MessageStateHandler m2 = Mockito.spy(new EmptyMessageStateHandler());

        queueManager.enqueueMessage(m1, m1, SCOPE_INSTANCE_ID, false);
        messages = queueManager.getNextMessages();
        Assert.assertEquals(m1, messages.get(0).handler);
        Assert.assertNull(messages.get(1));

        queueManager.enqueueMessage(m2, m2, SCOPE_INSTANCE_ID, false);
        messages = queueManager.getNextMessages();
        Assert.assertEquals(m1, messages.get(0).handler);
        Assert.assertEquals(m2, messages.get(1).handler);

        MessageStateHandler m3 = Mockito.spy(new EmptyMessageStateHandler());
        queueManager.enqueueMessage(m3, m3, SCOPE_INSTANCE_ID, true);
        messages = queueManager.getNextMessages();
        Assert.assertEquals(m3, messages.get(0).handler);
        Assert.assertEquals(m1, messages.get(1).handler);
    }

    @Test
    @SmallTest
    public void testIsLowerPriority() {
        MessageQueueManager queueManager = new MessageQueueManager(mAnimationCoordinator);
        // highPriority first but id is larger.
        Assert.assertFalse(
                "High-priority message has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(true, 2), buildMessageState(false, 1)));

        // highPriority first but id is smaller.
        Assert.assertFalse(
                "High-priority message has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(true, 2), buildMessageState(false, 3)));

        // highPriority second but id is larger.
        Assert.assertTrue(
                "High-priority message has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(false, 2), buildMessageState(true, 3)));

        // highPriority second but id is smaller.
        Assert.assertTrue(
                "High-priority message has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(false, 2), buildMessageState(true, 1)));

        // both high priority. Smaller id has a higher priority
        Assert.assertTrue(
                "Message with a smaller id has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(true, 2), buildMessageState(true, 1)));

        // both high priority. Smaller id has a higher priority
        Assert.assertFalse(
                "Message with a smaller id has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(true, 2), buildMessageState(true, 3)));

        // both normal priority. Smaller id has a higher priority
        Assert.assertTrue(
                "Message with a smaller id has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(false, 2), buildMessageState(false, 1)));

        // both normal priority. Smaller id has a higher priority
        Assert.assertFalse(
                "Message with a smaller id has a higher priority.",
                queueManager.isLowerPriority(
                        buildMessageState(false, 2), buildMessageState(false, 3)));
    }

    private MessageState buildMessageState(boolean highPriority, int id) {
        MessageStateHandler m1 = Mockito.spy(new EmptyMessageStateHandler());
        return new MessageState(SCOPE_INSTANCE_ID, m1, m1, highPriority, id);
    }
}
