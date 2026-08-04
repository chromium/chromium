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
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    public boolean hasContentToShow() {
        boolean hasContentToShow =
                mNativeSidePanelCoordinatorAndroid != 0
                        ? SidePanelCoordinatorAndroidImplJni.get()
                                .hasContentToShow(mNativeSidePanelCoordinatorAndroid)
                        : false;

        log(TAG, "hasContentToShow", hasContentToShow);
        return hasContentToShow;
    }

    @Override
    public void init() {
        log(TAG, "init");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get().init(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @Override
    public void onPanelContainerUpdated(int oldWidth, int newWidth) {
        log(TAG, "onPanelContainerUpdated", oldWidth, newWidth);
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onPanelContainerUpdated(
                            mNativeSidePanelCoordinatorAndroid, oldWidth, newWidth);
        }
    }

    @Override
    public void onPanelContentReplaced() {
        log(TAG, "onPanelContentReplaced");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onPanelContentReplaced(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @Override
    public void close() {
        log(TAG, "close");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get().closePanel(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @Override
    public void onWillAutoClose() {
        log(TAG, "onWillAutoClose");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onWillAutoClose(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @Override
    public void onWillAutoRestore() {
        log(TAG, "onWillAutoRestore");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .onWillAutoRestore(mNativeSidePanelCoordinatorAndroid);
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

    @CalledByNative
    private boolean canShow() {
        return mSidePanelContainerCoordinator.canShow();
    }

    @CalledByNative
    private void startOpeningPanel(
            View sidePanelNativeView,
            @JniType("std::u16string_view") String title,
            boolean shouldShowHeader,
            int x,
            int y,
            int width,
            int height,
            boolean suppressAnimations) {
        log(TAG, "startOpeningPanel", sidePanelNativeView, title, x, y, width, height);
        mSidePanelContainerCoordinator.startOpeningPanel(
                new SidePanelContent(sidePanelNativeView, title, shouldShowHeader),
                createRectFromCoordinates(x, y, width, height),
                suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNative
    private void startClosingPanel(boolean suppressAnimations) {
        log(TAG, "startClosingPanel", suppressAnimations);
        mSidePanelContainerCoordinator.startClosingPanel(
                suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNative
    private void startReplacingPanelContent(
            View sidePanelNativeView,
            @JniType("std::u16string_view") @Nullable String title,
            boolean shouldShowHeader) {
        log(TAG, "startReplacingPanelContent", sidePanelNativeView, title);
        mSidePanelContainerCoordinator.startReplacingPanelContent(
                new SidePanelContent(sidePanelNativeView, title, shouldShowHeader));
    }

    @CalledByNative
    private void endAnimations() {
        log(TAG, "endAnimations");
        mSidePanelContainerCoordinator.endAnimations();
    }

    @CalledByNative
    private void completePendingContentReplacement() {
        log(TAG, "completePendingContentReplacement");
        mSidePanelContainerCoordinator.completePendingContentReplacement();
    }

    @CalledByNativeForTesting
    private void configDeferredViewReplacementForTesting(boolean enable) {
        log(TAG, "configDeferredViewReplacementForTesting", enable);
        mSidePanelContainerCoordinator.configDeferredViewReplacementForTesting(enable); // IN-TEST
    }

    @CalledByNativeForTesting
    private void simulateAutoCloseConditionForTesting() {
        log(TAG, "simulateAutoCloseConditionForTesting");
        mSidePanelContainerCoordinator.simulateAutoCloseConditionForTesting(); // IN-TEST
    }

    @CalledByNativeForTesting
    private void simulateAutoRestoreConditionForTesting() {
        log(TAG, "simulateAutoRestoreConditionForTesting");
        mSidePanelContainerCoordinator.simulateAutoRestoreConditionForTesting(); // IN-TEST
    }

    @CalledByNativeForTesting
    private void disableAnimationsForTesting() {
        log(TAG, "disableAnimationsForTesting");
        mDisableAnimationsForTesting = true;
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
         * Closes the panel.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void closePanel(long nativeSidePanelCoordinatorAndroid);

        /**
         * Initializes the native coordinator and restores the active entry if one exists.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void init(long nativeSidePanelCoordinatorAndroid);

        /** See {@link SidePanelCoordinatorAndroid#hasContentToShow}. */
        boolean hasContentToShow(long nativeSidePanelCoordinatorAndroid);

        /** See {@link SidePanelCoordinatorAndroid#onPanelContainerUpdated}. */
        void onPanelContainerUpdated(
                long nativeSidePanelCoordinatorAndroid, int oldWidth, int newWidth);

        /** See {@link SidePanelCoordinatorAndroid#onPanelContentReplaced}. */
        void onPanelContentReplaced(long nativeSidePanelCoordinatorAndroid);

        /**
         * See {@link SidePanelCoordinatorAndroid#onWillAutoClose()}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void onWillAutoClose(long nativeSidePanelCoordinatorAndroid);

        /**
         * See {@link SidePanelCoordinatorAndroid#onWillAutoRestore()}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void onWillAutoRestore(long nativeSidePanelCoordinatorAndroid);
    }
}
