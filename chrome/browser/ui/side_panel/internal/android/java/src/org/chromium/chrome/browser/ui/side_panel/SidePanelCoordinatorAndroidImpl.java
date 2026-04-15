// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
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

    private final SidePanelContainerCoordinator mSidePanelContainerCoordinator;

    /** Address of the native {@code SidePanelCoordinatorAndroid}. */
    private long mNativeSidePanelCoordinatorAndroid;

    public SidePanelCoordinatorAndroidImpl(
            SidePanelContainerCoordinator sidePanelContainerCoordinator) {
        log(TAG, "constructor", sidePanelContainerCoordinator);
        mSidePanelContainerCoordinator = sidePanelContainerCoordinator;
    }

    @Override
    public void onAddedToTask(long nativeBrowserWindowPtr) {
        log(TAG, "onAddedToTask", nativeBrowserWindowPtr);
        createNativePtr(nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        log(TAG, "onFeatureRemoved");
        destroyNativePtr();
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
    private void populateSidePanel(View sidePanelNativeView) {
        log(TAG, "populateSidePanel", sidePanelNativeView);
        mSidePanelContainerCoordinator.populateContent(
                new SidePanelContent(sidePanelNativeView),
                result -> notifyOpenAnimationFinished(null));
    }

    @CalledByNative
    private void removeContentAndClose(boolean suppressAnimations) {
        @SidePanelType int type = mSidePanelContainerCoordinator.getPanelType();
        log(TAG, "removeContentAndClose", type, suppressAnimations);
        mSidePanelContainerCoordinator.removeContentAndClose(
                result -> notifyCloseAnimationFinished(null), suppressAnimations);
    }

    private void notifyOpenAnimationFinished(@Nullable Void unused) {
        @SidePanelType int type = mSidePanelContainerCoordinator.getPanelType();
        log(TAG, "notifyOpenAnimationFinished", type);
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .notifyOpenAnimationFinished(mNativeSidePanelCoordinatorAndroid, type);
        }
    }

    private void notifyCloseAnimationFinished(@Nullable Void unused) {
        @SidePanelType int type = mSidePanelContainerCoordinator.getPanelType();
        log(TAG, "notifyCloseAnimationFinished", type);
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidImplJni.get()
                    .notifyCloseAnimationFinished(mNativeSidePanelCoordinatorAndroid, type);
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
         * @param panelType SidePanelType of the current UI coordinator.
         */
        void notifyCloseAnimationFinished(
                long nativeSidePanelCoordinatorAndroid, @JniType("SidePanelType") int panelType);

        /**
         * Notifies the underlying native object that animations for opening have finished.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         * @param panelType SidePanelType of the current UI coordinator.
         */
        void notifyOpenAnimationFinished(
                long nativeSidePanelCoordinatorAndroid, @JniType("SidePanelType") int panelType);
    }
}
