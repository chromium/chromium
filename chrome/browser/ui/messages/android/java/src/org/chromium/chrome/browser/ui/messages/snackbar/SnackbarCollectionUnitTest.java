// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.graphics.Color;
import android.widget.FrameLayout;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.DismissalReason;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

import java.util.concurrent.TimeUnit;

/** Tests for {@link SnackbarCollection}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SnackbarCollectionUnitTest {
    private static final String ACTION_TITLE = "stack";
    private static final String NOTIFICATION_TITLE = "queue";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private SnackbarController mMockController;

    @Test
    public void testActionCoversNotification() {
        SnackbarCollection collection = new SnackbarCollection();
        assertTrue(collection.isEmpty());

        Snackbar notiBar = makeNotificationSnackbar();
        collection.add(notiBar);
        assertFalse(collection.isEmpty());
        assertEquals(notiBar, collection.getCurrent());

        Snackbar actionBar = makeActionSnackbar();
        collection.add(actionBar);
        verify(mMockController, times(1)).onDismissNoAction(null);
        assertFalse(collection.isEmpty());
        assertEquals(
                "Notification snackbar should not cover action snackbar!",
                actionBar,
                collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(mMockController, times(1)).onAction(null);
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testNotificationGoesUnderAction() {
        SnackbarCollection collection = new SnackbarCollection();
        assertTrue(collection.isEmpty());

        Snackbar actionBar = makeActionSnackbar();
        collection.add(actionBar);
        assertFalse(collection.isEmpty());
        assertEquals(actionBar, collection.getCurrent());

        Snackbar notiBar = makeNotificationSnackbar();
        collection.add(notiBar);
        verify(mMockController, times(0)).onDismissNoAction(null);
        assertFalse(collection.isEmpty());
        assertEquals(
                "Action snackbar should not be covered by notification snackbars!",
                actionBar,
                collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(mMockController, times(1)).onAction(null);
        assertFalse(collection.isEmpty());
        assertEquals(notiBar, collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(mMockController, times(2)).onAction(null);
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testClear() {
        SnackbarCollection collection = new SnackbarCollection();
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar());
            collection.add(makeNotificationSnackbar());
        }
        assertFalse(collection.isEmpty());

        collection.clear();
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testRemoveMatchingSnackbars() {
        SnackbarCollection collection = new SnackbarCollection();
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar());
            collection.add(makeNotificationSnackbar());
        }
        SnackbarController anotherController = mock(SnackbarController.class);
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar(anotherController));
            collection.add(makeNotificationSnackbar(anotherController));
        }

        collection.removeMatchingSnackbars(mMockController);
        while (!collection.isEmpty()) {
            Snackbar removed = collection.removeCurrentDueToAction();
            assertEquals(anotherController, removed.getController());
        }
    }

    @Test
    public void testRemoveMatchingSnackbarsWithData() {
        SnackbarCollection collection = new SnackbarCollection();
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar().setAction(ACTION_TITLE, i));
            collection.add(makeNotificationSnackbar().setAction(NOTIFICATION_TITLE, i));
        }
        SnackbarController anotherController = mock(SnackbarController.class);
        for (int i = 0; i < 3; i++) {
            collection.add(makeActionSnackbar(anotherController).setAction(ACTION_TITLE, i));
            collection.add(
                    makeNotificationSnackbar(anotherController).setAction(NOTIFICATION_TITLE, i));
        }

        final Integer dataToRemove = 0;
        collection.removeMatchingSnackbars(mMockController, dataToRemove);
        while (!collection.isEmpty()) {
            Snackbar removed = collection.removeCurrentDueToAction();
            assertFalse(
                    mMockController == removed.getController()
                            && dataToRemove.equals(removed.getActionData()));
        }
    }

    // This test is added as a result of crbug.com/40796914.
    // Test that the action/dismiss callbacks are not invoked when the controller is null.
    @Test
    public void testRemoveCurrent_ControllerNotSpecified() {
        SnackbarCollection collection = new SnackbarCollection();
        assertTrue(collection.isEmpty());

        Snackbar snackbar =
                Snackbar.make(
                        NOTIFICATION_TITLE,
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_TEST_SNACKBAR);
        collection.add(snackbar);
        assertFalse("Snackbar collection should contain a snackbar.", collection.isEmpty());
        assertEquals(
                "The currently displayed snackbar is incorrect.",
                snackbar,
                collection.getCurrent());
        collection.removeCurrentDueToTimeout();
        verifyNoMoreInteractions(mMockController);
    }

    @Test
    public void testDismissalReasons() {
        SnackbarCollection collection = new SnackbarCollection();

        // Action
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Snackbar.DismissalReason", DismissalReason.ACTION_BUTTON)
                        .build();
        collection.add(makeActionSnackbar());
        collection.removeCurrentDueToAction();
        watcher.assertExpected("Action button dismissal should be recorded.");

        // Timeout
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Snackbar.DismissalReason", DismissalReason.TIMEOUT)
                        .build();
        collection.add(makeActionSnackbar());
        collection.removeCurrentDueToTimeout();
        watcher.assertExpected("Timeout dismissal should be recorded.");

        // Swipe
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Snackbar.DismissalReason", DismissalReason.SWIPE)
                        .build();
        collection.add(makeActionSnackbar());
        collection.removeCurrentDueToSwipe();
        watcher.assertExpected("Swipe dismissal should be recorded.");

        // Dismissed by caller
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Snackbar.DismissalReason", DismissalReason.DISMISSED_BY_CALLER)
                        .build();
        collection.add(makeNotificationSnackbar());
        collection.removeMatchingSnackbars(mMockController);
        watcher.assertExpected("Dismissed by caller should be recorded.");

        // Others
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Snackbar.DismissalReason", DismissalReason.OTHERS)
                        .build();
        collection.add(makeActionSnackbar());
        collection.clear();
        watcher.assertExpected("Others dismissal should be recorded.");

        // Replaced by action snackbar
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Snackbar.DismissalReason",
                                DismissalReason.REPLACED_BY_ACTION_SNACKBAR)
                        .build();
        collection.add(makeNotificationSnackbar());
        collection.add(makeActionSnackbar());
        watcher.assertExpected("Replaced by action snackbar should be recorded.");
    }

    @Test
    public void testHighPriorityActionSnackbarNotDiscardedOnTimeout() {
        SnackbarCollection collection = new SnackbarCollection();
        assertTrue(collection.isEmpty());

        // High priority snackbar
        Snackbar highPriorityBar =
                makeActionSnackbar().setHighPriority(true).setAction("highPriority", null);
        collection.add(highPriorityBar);

        // Regular action snackbar added later.
        Snackbar actionBar = makeActionSnackbar().setAction("action", null);
        collection.add(actionBar);

        // High priority snackbar should remain at the front and NOT be covered.
        assertEquals(highPriorityBar, collection.getCurrent());

        // Timeout on highPriorityBar should NOT discard actionBar
        collection.removeCurrentDueToTimeout();
        verify(mMockController, times(1)).onDismissNoAction(null);
        assertFalse(collection.isEmpty());
        assertEquals(
                "Regular action snackbar should now be current",
                actionBar,
                collection.getCurrent());

        collection.removeCurrentDueToTimeout();
        verify(mMockController, times(2)).onDismissNoAction(null);
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testMultipleHighPrioritySnackbarsShownSequentially() {
        SnackbarCollection collection = new SnackbarCollection();

        SnackbarController controller1 = mock(SnackbarController.class);
        Snackbar hp1 =
                Snackbar.make(
                                ACTION_TITLE,
                                controller1,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_TEST_SNACKBAR)
                        .setHighPriority(true);
        collection.add(hp1);

        SnackbarController controller2 = mock(SnackbarController.class);
        Snackbar hp2 =
                Snackbar.make(
                                ACTION_TITLE,
                                controller2,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_TEST_SNACKBAR)
                        .setHighPriority(true);
        collection.add(hp2);

        // hp2 should instantly interrupt and replace hp1.
        assertEquals(hp2, collection.getCurrent());
        // Verify hp1 was dismissed with the correct reason.
        verify(controller1, times(1)).onDismissNoAction(null);

        // Remove hp2
        collection.removeMatchingSnackbars(controller2);
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testQueueExhaustion_HighPriorityPreemptsSpam() {
        SnackbarCollection collection = new SnackbarCollection();
        SnackbarController spamController = mock(SnackbarController.class);

        // 1. Attacker spams the queue with standard notifications (like "Copied to clipboard")
        for (int i = 0; i < 20; ++i) {
            Snackbar spam =
                    Snackbar.make(
                            "Spam " + i,
                            spamController,
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_TEST_SNACKBAR);
            collection.add(spam);
        }

        // At this point, the queue is full of spam.
        // 2. A legitimate High Priority warning (Fullscreen) is triggered.
        SnackbarController hpController = mock(SnackbarController.class);
        Snackbar hpSnackbar =
                Snackbar.make(
                                "Fullscreen Warning",
                                hpController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_TEST_SNACKBAR)
                        .setHighPriority(true);
        collection.add(hpSnackbar);

        // The HP Snackbar must instantly become the current visible Snackbar,
        // bypassing the spam messages waiting in the queue.
        assertEquals(hpSnackbar, collection.getCurrent());
    }

    @Test
    public void testQueueExhaustion_EvictsOldestUnseen() {
        SnackbarCollection collection = new SnackbarCollection();
        SnackbarController[] controllers = new SnackbarController[15];

        // 1. Attacker spams the queue with 15 standard notifications.
        for (int i = 0; i < 15; ++i) {
            controllers[i] = mock(SnackbarController.class);
            Snackbar spam =
                    Snackbar.make(
                            "Spam " + i,
                            controllers[i],
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_TEST_SNACKBAR);
            collection.add(spam);
        }

        // 2. The currently showing snackbar (index 0) must be skipped during eviction.
        // It should still be the current snackbar.
        Snackbar current = collection.getCurrent();
        org.junit.Assert.assertNotNull(current);
        assertEquals(controllers[0], current.getController());

        // 3. The queue is capped at MAX_SNACKBARS (10).
        // Since 15 were added, exactly 5 must be evicted.
        // The evicted ones must be the OLDEST UNSEEN (indices 1, 2, 3, 4, 5).
        for (int i = 1; i <= 5; ++i) {
            verify(controllers[i], times(1)).onDismissNoAction(null);
        }

        // The remaining items (indices 6 through 14) should NOT have been evicted yet.
        for (int i = 6; i < 15; ++i) {
            verify(controllers[i], org.mockito.Mockito.never()).onDismissNoAction(null);
        }
    }

    @Test
    public void testOscillationAttack_StateDeduplication() {
        SnackbarCollection collection = new SnackbarCollection();
        SnackbarController controller = mock(SnackbarController.class);
        Object actionData = new Object();

        // 1. Attacker rapidly requests Fullscreen 5 times in a row.
        for (int i = 0; i < 5; ++i) {
            Snackbar hpSnackbar =
                    Snackbar.make(
                                    "Fullscreen",
                                    controller,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_TEST_SNACKBAR)
                            .setAction("Undo", actionData)
                            .setHighPriority(true);

            collection.add(hpSnackbar);
        }

        // The queue must only contain ONE instance of the HP warning.
        Snackbar current = collection.getCurrent();
        org.junit.Assert.assertNotNull(current);
        collection.removeMatchingSnackbars(current.getController(), current.getActionData());
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testHighPriorityUpdateWithUpdatedText() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout parent = new FrameLayout(activity);
        activity.setContentView(parent);
        SnackbarManager manager = new SnackbarManager(activity, parent, null, null, null);

        Snackbar hp1 =
                Snackbar.make(
                                "Press Esc",
                                mMockController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setBackgroundColor(Color.BLACK)
                        .setHighPriority(true);
        manager.showSnackbar(hp1);
        assertEquals(hp1, manager.getCurrentSnackbarForTesting());
        assertEquals("Press Esc", manager.getCurrentSnackbarForTesting().getText());

        // Advance 7000ms into the 10000ms TYPE_ACTION timeout (3000ms remaining).
        ShadowLooper.idleMainLooper(7000, TimeUnit.MILLISECONDS);
        assertTrue(manager.isShowing());

        Snackbar hp2 =
                Snackbar.make(
                                "Press and hold Esc",
                                mMockController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setBackgroundColor(Color.BLACK)
                        .setHighPriority(true);
        manager.showSnackbar(hp2);

        // Verify hp1 was updated to hp2 without dismissing the controller.
        verify(mMockController, never()).onDismissNoAction(any());
        assertEquals(hp2, manager.getCurrentSnackbarForTesting());
        assertEquals("Press and hold Esc", manager.getCurrentSnackbarForTesting().getText());

        // Advance another 7000ms (14000ms total elapsed > initial 10000ms duration).
        // Because the duration timeout was reset on update, the snackbar must still be showing.
        ShadowLooper.idleMainLooper(7000, TimeUnit.MILLISECONDS);
        verify(mMockController, never()).onDismissNoAction(any());
        assertTrue(manager.isShowing());
        assertEquals(hp2, manager.getCurrentSnackbarForTesting());

        // Advance past the remaining 3000ms of the reset 10000ms duration; now it should time out.
        ShadowLooper.idleMainLooper(3500, TimeUnit.MILLISECONDS);
        verify(mMockController, times(1)).onDismissNoAction(null);
        assertFalse(manager.isShowing());
        manager.destroy();
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testHighPriorityUpdateWithUpdatedTemplateText() {
        SnackbarCollection collection = new SnackbarCollection();

        Snackbar hp1 =
                Snackbar.make(
                                "Title",
                                mMockController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setTemplateText("Template %s")
                        .setHighPriority(true)
                        .setDuration(3800);
        collection.add(hp1);
        assertEquals(hp1, collection.getCurrent());

        Snackbar hp2 =
                Snackbar.make(
                                "Title",
                                mMockController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setTemplateText("Updated Template %s")
                        .setHighPriority(true)
                        .setDuration(5000);
        collection.add(hp2);

        verify(mMockController, never()).onDismissNoAction(any());
        assertEquals(hp2, collection.getCurrent());
        assertEquals(5000, collection.getCurrent().getDuration());
    }

    @Test
    public void testHighPriorityDeduplicationWithIdenticalText() {
        SnackbarCollection collection = new SnackbarCollection();

        Snackbar hp1 =
                Snackbar.make(
                                "Press Esc",
                                mMockController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setHighPriority(true);
        collection.add(hp1);
        assertEquals(hp1, collection.getCurrent());

        Snackbar hp2 =
                Snackbar.make(
                                "Press Esc",
                                mMockController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setHighPriority(true);
        collection.add(hp2);

        // Deduplicated without dismissal: the existing instance hp1 should be retained.
        verify(mMockController, never()).onDismissNoAction(any());
        assertSame(hp1, collection.getCurrent());
    }

    @Test
    public void testPersistentHighPriorityRestoredAfterActionPreemption() {
        SnackbarCollection collection = new SnackbarCollection();
        SnackbarController persistentController = mock(SnackbarController.class);
        SnackbarController actionController = mock(SnackbarController.class);

        Snackbar persistentHp = makePersistentHpSnackbar(persistentController);
        collection.add(persistentHp);
        assertEquals(persistentHp, collection.getCurrent());

        Snackbar actionHp =
                Snackbar.make(
                                "Press Esc to exit fullscreen",
                                actionController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setHighPriority(true);
        collection.add(actionHp);
        assertEquals(actionHp, collection.getCurrent());
        verify(persistentController, org.mockito.Mockito.never()).onDismissNoAction(null);

        collection.removeCurrentDueToTimeout();
        verify(actionController, times(1)).onDismissNoAction(null);
        assertEquals(persistentHp, collection.getCurrent());

        collection.removeCurrentDueToAction();
        verify(persistentController, times(1)).onAction(null);
        assertTrue(collection.isEmpty());
    }

    @Test
    public void testPersistentHighPriorityNotBuriedOrEvictedByNormalPersistentSnackbars() {
        SnackbarCollection collection = new SnackbarCollection();
        SnackbarController persistentController = mock(SnackbarController.class);
        SnackbarController actionController = mock(SnackbarController.class);

        Snackbar persistentHp = makePersistentHpSnackbar(persistentController);
        collection.add(persistentHp);

        Snackbar actionHp =
                Snackbar.make(
                                "Press Esc to exit fullscreen",
                                actionController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        .setHighPriority(true);
        collection.add(actionHp);
        assertEquals(actionHp, collection.getCurrent());

        // Add more normal persistent snackbars than MAX_SNACKBARS (10) to trigger queue eviction.
        for (int i = 0; i < 15; i++) {
            Snackbar normalPersistent =
                    makePersistentSnackbar(
                            mock(SnackbarController.class), "Normal persistent " + i);
            collection.add(normalPersistent);
        }

        // Action HP must still be current.
        assertEquals(actionHp, collection.getCurrent());

        // When action HP times out, persistent HP must be restored, not buried or evicted.
        collection.removeCurrentDueToTimeout();
        assertEquals(persistentHp, collection.getCurrent());
        verify(persistentController, org.mockito.Mockito.never()).onDismissNoAction(null);
    }

    @Test
    public void testNormalActionSnackbarDoesNotDismissPersistentHighPriority() {
        SnackbarCollection collection = new SnackbarCollection();
        SnackbarController persistentController = mock(SnackbarController.class);
        SnackbarController normalActionController = mock(SnackbarController.class);

        Snackbar persistentHp = makePersistentHpSnackbar(persistentController);
        collection.add(persistentHp);
        assertEquals(persistentHp, collection.getCurrent());

        // Add a normal, non-HP action snackbar. It must not dismiss the persistent HP disclosure.
        Snackbar normalAction = makeActionSnackbar(normalActionController);
        collection.add(normalAction);

        verify(persistentController, org.mockito.Mockito.never()).onDismissNoAction(null);
        assertEquals(persistentHp, collection.getCurrent());

        // Dismiss persistent HP via user action; the normal action snackbar should now be visible.
        collection.removeCurrentDueToAction();
        verify(persistentController, times(1)).onAction(null);
        assertEquals(normalAction, collection.getCurrent());
    }

    private Snackbar makePersistentHpSnackbar(SnackbarController controller) {
        return Snackbar.make(
                        "Running in Chrome",
                        controller,
                        Snackbar.TYPE_PERSISTENT,
                        Snackbar.UMA_TWA_PRIVACY_DISCLOSURE)
                .setHighPriority(true)
                .setAction("Got it", null);
    }

    private Snackbar makePersistentSnackbar(SnackbarController controller, String text) {
        return Snackbar.make(text, controller, Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR)
                .setAction("Action", null);
    }

    private Snackbar makeActionSnackbar(SnackbarController controller) {
        return Snackbar.make(
                ACTION_TITLE, controller, Snackbar.TYPE_ACTION, Snackbar.UMA_TEST_SNACKBAR);
    }

    private Snackbar makeNotificationSnackbar(SnackbarController controller) {
        return Snackbar.make(
                NOTIFICATION_TITLE,
                controller,
                Snackbar.TYPE_NOTIFICATION,
                Snackbar.UMA_TEST_SNACKBAR);
    }

    private Snackbar makeActionSnackbar() {
        return makeActionSnackbar(mMockController);
    }

    private Snackbar makeNotificationSnackbar() {
        return makeNotificationSnackbar(mMockController);
    }
}
