// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Notifies {@link AndroidBrowserWindowObserver}s of {@link AndroidBrowserWindow} lifecycle events.
 */
@NullMarked
final class AndroidBrowserWindowObserverNotifier {
    private final ObserverList<AndroidBrowserWindowObserver> mObservers = new ObserverList<>();

    /** The last active {@link AndroidBrowserWindow}. */
    @Nullable private AndroidBrowserWindow mLastActiveBrowserWindow;

    AndroidBrowserWindowObserverNotifier() {}

    void addObserver(AndroidBrowserWindowObserver observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(AndroidBrowserWindowObserver observer) {
        mObservers.removeObserver(observer);
    }

    boolean hasObserver(AndroidBrowserWindowObserver observer) {
        return mObservers.hasObserver(observer);
    }

    void notifyBrowserWindowAdded(AndroidBrowserWindow window) {
        long ptr = window.getOrCreateNativePtr();
        for (var observer : mObservers) {
            observer.onBrowserWindowAdded(ptr);
        }
    }

    void notifyBrowserWindowDestroyed(AndroidBrowserWindow window) {
        // If the destroyed window was the active window, update the active window.
        // Do NOT notify observers of deactivation to match WML behavior.
        if (mLastActiveBrowserWindow == window) {
            mLastActiveBrowserWindow = null;
        }

        // Notify observers of window destruction.
        long ptr = window.getNativePtr();
        assert ptr != 0;
        for (var observer : mObservers) {
            observer.onBrowserWindowRemoved(ptr);
        }
    }

    void updateActiveBrowserWindow(@Nullable AndroidBrowserWindow activeWindow) {
        if (mLastActiveBrowserWindow == activeWindow) return;

        if (mLastActiveBrowserWindow != null && mLastActiveBrowserWindow.getNativePtr() != 0) {
            for (var observer : mObservers) {
                observer.onBrowserWindowDeactivated(mLastActiveBrowserWindow.getNativePtr());
            }
        }
        mLastActiveBrowserWindow = activeWindow;
        if (mLastActiveBrowserWindow != null && mLastActiveBrowserWindow.getNativePtr() != 0) {
            for (var observer : mObservers) {
                observer.onBrowserWindowActivated(mLastActiveBrowserWindow.getNativePtr());
            }
        }
    }
}
