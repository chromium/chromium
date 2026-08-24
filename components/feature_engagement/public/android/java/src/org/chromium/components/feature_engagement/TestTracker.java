// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A fake {@link Tracker} that provides default no-op behavior for tests. Prefer this fake over an
 * {@code @Mock} in integration tests to avoid Mockito setup overhead and callback memory leaks.
 */
@NullMarked
public class TestTracker implements Tracker {
    @Override
    public void notifyEvent(String event) {}

    @Override
    public boolean shouldTriggerHelpUi(String feature) {
        return false;
    }

    @Override
    public TriggerDetails shouldTriggerHelpUiWithSnooze(String feature) {
        return new TriggerDetails(false, false);
    }

    @Override
    public boolean wouldTriggerHelpUi(String feature) {
        return false;
    }

    @Override
    public boolean hasEverTriggered(String feature, boolean fromWindow) {
        return false;
    }

    @Override
    public @TriggerState int getTriggerState(String feature) {
        return TriggerState.NOT_READY;
    }

    @Override
    public void dismissed(String feature) {}

    @Override
    public void dismissedWithSnooze(String feature, int snoozeAction) {}

    @Override
    public @Nullable DisplayLockHandle acquireDisplayLock() {
        return () -> {};
    }

    @Override
    public void setPriorityNotification(String feature) {}

    @Override
    public @Nullable String getPendingPriorityNotification() {
        return null;
    }

    @Override
    public void registerPriorityNotificationHandler(
            String feature, Runnable priorityNotificationHandler) {}

    @Override
    public void unregisterPriorityNotificationHandler(String feature) {}

    @Override
    public boolean isInitialized() {
        return true;
    }

    @Override
    public void addOnInitializedCallback(Callback<Boolean> callback) {
        callback.onResult(true);
    }
}
