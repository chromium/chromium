// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.Objects;

/** Observer interface for {@code AndroidBrowserWindow} lifecycle. */
@NullMarked
public interface AndroidBrowserWindowObserver {

    /**
     * Information about a native {@code AndroidBrowserWindow}.
     *
     * <p>Note: Please do <i>not</i> use this class to control the lifecycle of the {@code
     * AndroidBrowserWindow}. This class intentionally makes it hard to do so.
     */
    final class AndroidBrowserWindowInfo {

        /** Address of the native {@code AndroidBrowserWindow}. */
        public final long mBrowserWindowPtr;

        /**
         * {@link Profile} of the native {@code AndroidBrowserWindow}.
         *
         * <p>A native {@code AndroidBrowserWindow} is guaranteed to have a single {@link Profile}
         * during its lifetime. This matches the expectation of {@code AndroidBrowserWindow}'s
         * cross-platform interface ({@code BrowserWindowInterface}).
         */
        public final Profile mProfile;

        /**
         * {@link ActivityWindowAndroid} hosting the native {@code AndroidBrowserWindow}.
         *
         * <p>An {@link ActivityWindowAndroid} may host more than one {@code AndroidBrowserWindow},
         * such as when the {@code Activity} supports both regular and incognito tabs.
         *
         * <p>When this is null, the {@code Activity} is still being created. This is the "pending"
         * state when an {@code AndroidBrowserWindow} has been created by the native {@code
         * CreateBrowserWindow()} function, but the {@code Activity} hasn't become alive.
         */
        public final @Nullable ActivityWindowAndroid mActivityWindowAndroid;

        /**
         * Creates {@link AndroidBrowserWindowInfo}.
         *
         * <p>The constructor is intentionally made package-private since no code outside this
         * package should create {@link AndroidBrowserWindowInfo}.
         *
         * @param browserWindowPtr See {@link AndroidBrowserWindowInfo#mBrowserWindowPtr}.
         * @param profile See {@link AndroidBrowserWindowInfo#mProfile}.
         * @param activityWindowAndroid See {@link AndroidBrowserWindowInfo#mActivityWindowAndroid}.
         */
        AndroidBrowserWindowInfo(
                long browserWindowPtr,
                Profile profile,
                @Nullable ActivityWindowAndroid activityWindowAndroid) {
            mBrowserWindowPtr = browserWindowPtr;
            mProfile = profile;
            mActivityWindowAndroid = activityWindowAndroid;
        }

        @Override
        public boolean equals(@Nullable Object o) {
            if (o == this) {
                return true;
            }

            if (o instanceof AndroidBrowserWindowInfo other) {
                return mBrowserWindowPtr == other.mBrowserWindowPtr
                        && Objects.equals(mProfile, other.mProfile)
                        && Objects.equals(mActivityWindowAndroid, other.mActivityWindowAndroid);
            }

            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mBrowserWindowPtr, mProfile, mActivityWindowAndroid);
        }
    }

    /**
     * Called when an {@code AndroidBrowserWindow} is added.
     *
     * @param windowInfo The {@link AndroidBrowserWindowInfo} for the {@code AndroidBrowserWindow}.
     */
    void onBrowserWindowAdded(AndroidBrowserWindowInfo windowInfo);

    /**
     * Called when an {@code AndroidBrowserWindow} is removed.
     *
     * @param windowInfo The {@link AndroidBrowserWindowInfo} for the {@code AndroidBrowserWindow}.
     */
    void onBrowserWindowRemoved(AndroidBrowserWindowInfo windowInfo);

    /**
     * Called when an {@code AndroidBrowserWindow} becomes the active window (e.g., when switching
     * to incognito, or resuming from background).
     *
     * @param windowInfo The {@link AndroidBrowserWindowInfo} for the {@code AndroidBrowserWindow}.
     */
    default void onBrowserWindowActivated(AndroidBrowserWindowInfo windowInfo) {}

    /**
     * Called when an {@code AndroidBrowserWindow} is no longer the active window (e.g., when
     * switching away from incognito or backgrounding the app).
     *
     * @param windowInfo The {@link AndroidBrowserWindowInfo} for the {@code AndroidBrowserWindow}.
     */
    default void onBrowserWindowDeactivated(AndroidBrowserWindowInfo windowInfo) {}
}
