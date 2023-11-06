// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenuButton.PopupMenuShownListener;

/** Unit tests for MessageBannerCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageBannerCoordinatorUnitTest {
    @Test
    public void testCreatePopupMenuShownListener() {
        MessageBannerCoordinator coordinator = Mockito.mock(MessageBannerCoordinator.class);
        MessageAutoDismissTimer timer = Mockito.mock(MessageAutoDismissTimer.class);
        Runnable onTimeUp = () -> {};
        long durationMs = 10000L;
        Mockito.when(coordinator.createPopupMenuShownListener(timer, durationMs, onTimeUp))
                .thenCallRealMethod();
        PopupMenuShownListener listener =
                coordinator.createPopupMenuShownListener(timer, durationMs, onTimeUp);

        // Invoke #onPopupMenuShown, verify that the timer is cancelled.
        listener.onPopupMenuShown();
        Mockito.verify(timer).cancelTimer();

        // Invoke #onPopupMenuDismissed, verify that the timer is (re)started.
        listener.onPopupMenuDismissed();
        Mockito.verify(timer).startTimer(durationMs, onTimeUp);
    }
}
