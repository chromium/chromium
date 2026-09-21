// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowAccessibilityManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenuHost.PopupMenuShownListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.RunnableTimer;

import java.util.List;

/** Unit tests for MessageBannerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageBannerCoordinatorUnitTest {
    @Test
    public void testCreatePopupMenuShownListener() {
        MessageBannerCoordinator coordinator = mock(MessageBannerCoordinator.class);
        RunnableTimer timer = mock(RunnableTimer.class);
        Runnable onTimeUp = () -> {};
        long durationMs = 10000L;
        when(coordinator.createPopupMenuShownListener(timer, durationMs, onTimeUp))
                .thenCallRealMethod();
        PopupMenuShownListener listener =
                coordinator.createPopupMenuShownListener(timer, durationMs, onTimeUp);

        // Invoke #onPopupMenuShown, verify that the timer is cancelled.
        listener.onPopupMenuShown();
        verify(timer).cancelTimer();

        // Invoke #onPopupMenuDismissed, verify that the timer is (re)started.
        listener.onPopupMenuDismissed();
        verify(timer).startTimer(durationMs, onTimeUp);
    }

    @Test
    public void testSendPaneChangeAccessibilityEvent() {
        MessageBannerView view = mock(MessageBannerView.class);
        PropertyModel model = new PropertyModel(MessageBannerProperties.ALL_KEYS);
        View parentView = mock(View.class);
        Resources resources = mock(Resources.class);
        when(resources.getDisplayMetrics()).thenReturn(new DisplayMetrics());

        MessageBannerCoordinator coordinator =
                new MessageBannerCoordinator(
                        view,
                        model,
                        /* maxTranslationSupplier= */ () -> 0,
                        /* topOffsetSupplier= */ () -> 0,
                        resources,
                        parentView,
                        /* messageDismissed= */ () -> {},
                        mock(SwipeAnimationHandler.class),
                        /* autodismissDurationMs= */ () -> 0L,
                        /* onTimeUp= */ () -> {});

        Context context = ContextUtils.getApplicationContext();
        AccessibilityManager manager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        ShadowAccessibilityManager shadowManager = shadowOf(manager);
        shadowManager.setEnabled(true);

        coordinator.sendPaneChangeAccessibilityEvent(true);

        List<AccessibilityEvent> events = shadowManager.getSentAccessibilityEvents();
        assertEquals(1, events.size());
        AccessibilityEvent event = events.get(0);
        assertEquals(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED, event.getEventType());
        assertEquals(view, shadowOf(event).getSourceRoot());
    }
}
