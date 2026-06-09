// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContent;

/** Implements {@code SidePanelCoordinatorAndroid}. */
@NullMarked
public final class SidePanelCoordinatorAndroidImpl implements SidePanelCoordinatorAndroid {
    private static final String TAG = "SidePanelCoordinatorAndroidImpl";

    /** Sentinel value for invalid or unset coordinates. */
    private static final int INVALID_COORDINATE = -1;

    private final SidePanelContainerCoordinator mSidePanelContainerCoordinator;

    /** Address of the native {@code SidePanelCoordinatorAndroid}. */
    private long mNativeSidePanelCoordinatorAndroid;

    private boolean mDisableAnimationsForTesting;

    public SidePanelCoordinatorAndroidImpl(
            SidePanelContainerCoordinator sidePanelContainerCoordinator) {
        log(TAG, "constructor", sidePanelContainerCoordinator);
        mSidePanelContainerCoordinator = sidePanelContainerCoordinator;
    }

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        long nativeBrowserWindowPtr = initInfo.nativeBrowserWindowPtr;
        log(TAG, "onAddedToTask", nativeBrowserWindowPtr);
        createNativePtr(nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        log(TAG, "onFeatureRemoved");
        destroyNativePtr();
    }

    @Override
    public void onWindowResized(boolean canShowSidePanel) {
        log(TAG, "onWindowResized", canShowSidePanel);
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onWindowResized(mNativeSidePanelCoordinatorAndroid, canShowSidePanel);
        }
    }

    @VisibleForTesting
    void createNativePtr(long nativeBrowserWindowPtr) {
        log(TAG, "createNativePtr", nativeBrowserWindowPtr);
        assert nativeBrowserWindowPtr != 0
                : "Native BrowserWindowInterface pointer shouldn't be null. Is the"
                        + " ChromeAndroidTaskFeatureKey correct?";
        assert mNativeSidePanelCoordinatorAndroid == 0
                : "Native SidePanelCoordinatorAndroid already exists";
        mNativeSidePanelCoordinatorAndroid =
                SidePanelCoordinatorAndroidImplJni.get().create(this, nativeBrowserWindowPtr);
    }

    @VisibleForTesting
    void destroyNativePtr() {
        log(TAG, "destroyNativePtr");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get().destroy(mNativeSidePanelCoordinatorAndroid);
        }
    }

    long getNativePtrForTesting() {
        return mNativeSidePanelCoordinatorAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        log(TAG, "clearNativePtr");
        mNativeSidePanelCoordinatorAndroid = 0;
    }

    @CalledByNativeForTesting
    private void disableAnimationsForTesting() {
        log(TAG, "disableAnimationsForTesting");
        mDisableAnimationsForTesting = true;
    }

    /**
     * Populates the side panel with content.
     *
     * @param sidePanelNativeView The view to show.
     * @param x The x coordinate of the starting bounds, or -1 if none.
     * @param y The y coordinate of the starting bounds, or -1 if none.
     * @param width The width of the starting bounds, or -1 if none.
     * @param height The height of the starting bounds, or -1 if none.
     */
    @CalledByNative
    private void populateSidePanel(
            View sidePanelNativeView,
            int x,
            int y,
            int width,
            int height,
            boolean suppressAnimations) {
        log(TAG, "populateSidePanel", sidePanelNativeView, x, y, width, height);
        mSidePanelContainerCoordinator.populateContent(
                new SidePanelContent(sidePanelNativeView),
                result -> notifyOpenAnimationFinished(null),
                createRectFromCoordinates(x, y, width, height),
                suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNative
    private void removeContentAndClose(boolean suppressAnimations) {
        log(TAG, "removeContentAndClose", suppressAnimations);
        mSidePanelContainerCoordinator.removeContentAndClose(
                result -> notifyCloseAnimationFinished(null),
                suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNativeForTesting
    private int getContainerWidthForTesting() {
        View view = mSidePanelContainerCoordinator.getViewForTesting(); // IN-TEST
        if (view == null || !view.isAttachedToWindow()) {
            return 0;
        }
        return view.getWidth();
    }

    private @Nullable Rect createRectFromCoordinates(int x, int y, int width, int height) {
        if (x == INVALID_COORDINATE
                && y == INVALID_COORDINATE
                && width == INVALID_COORDINATE
                && height == INVALID_COORDINATE) {
            return null;
        }
        return new Rect(x, y, x + width, y + height);
    }

    private void notifyOpenAnimationFinished(@Nullable Void unused) {
        log(TAG, "notifyOpenAnimationFinished");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .notifyOpenAnimationFinished(mNativeSidePanelCoordinatorAndroid);
        }
    }

    private void notifyCloseAnimationFinished(@Nullable Void unused) {
        log(TAG, "notifyCloseAnimationFinished");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .notifyCloseAnimationFinished(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code SidePanelCoordinatorAndroid}.
         *
         * @param caller The Java object calling this method.
         * @param nativeBrowserWindowPtr The pointer to the native {@code BrowserWindowInterface}.
         * @return The address of the native {@code SidePanelCoordinatorAndroid}.
         */
        long create(SidePanelCoordinatorAndroidImpl caller, long nativeBrowserWindowPtr);

        /**
         * Destroys the native {@code SidePanelCoordinatorAndroid}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void destroy(long nativeSidePanelCoordinatorAndroid);

        /**
         * Notifies the underlying native object that animations for closing have finished.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void notifyCloseAnimationFinished(long nativeSidePanelCoordinatorAndroid);

        /**
         * Notifies the underlying native object that animations for opening have finished.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void notifyOpenAnimationFinished(long nativeSidePanelCoordinatorAndroid);

        /**
         * See {@link SidePanelCoordinatorAndroid#onWindowResized(boolean).
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         * @param canShowSidePanel Whether the side panel can be shown.
         */
        void onWindowResized(long nativeSidePanelCoordinatorAndroid, boolean canShowSidePanel);
    }
}
