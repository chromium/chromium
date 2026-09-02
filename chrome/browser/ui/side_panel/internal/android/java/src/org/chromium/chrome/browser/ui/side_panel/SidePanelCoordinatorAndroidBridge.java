// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelUtils.log;

import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/** JNI bridge for communicating with the native {@code SidePanelCoordinatorAndroid}. */
@NullMarked
final class SidePanelCoordinatorAndroidBridge implements ChromeAndroidTaskFeature {
    private static final String TAG = "SidePanelCoordinatorAndroidBridge";

    private final SidePanelNativeBridgeSelector mNativeBridgeSelector;

    /** Address of the native {@code SidePanelCoordinatorAndroid}. */
    private long mNativeSidePanelCoordinatorAndroid;

    private boolean mDisableAnimationsForTesting;

    SidePanelCoordinatorAndroidBridge(SidePanelNativeBridgeSelector nativeBridgeSelector) {
        log(TAG, "constructor");
        mNativeBridgeSelector = nativeBridgeSelector;
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

    /**
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#hasContentToShow(Tab)
     */
    boolean hasContentToShow(Tab tab) {
        boolean hasContentToShow =
                mNativeSidePanelCoordinatorAndroid != 0
                        ? SidePanelCoordinatorAndroidBridgeJni.get()
                                .hasContentToShow(mNativeSidePanelCoordinatorAndroid, tab)
                        : false;

        log(TAG, "hasContentToShow", hasContentToShow, "Tab#" + tab.getId());
        return hasContentToShow;
    }

    /** Initializes the native coordinator and restores the active entry if one exists. */
    void init() {
        log(TAG, "init");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get().init(mNativeSidePanelCoordinatorAndroid);
        }
    }

    /**
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#onUiUpdateCompleted
     */
    void onPanelContainerUpdated(int oldWidth, int newWidth) {
        log(TAG, "onPanelContainerUpdated", oldWidth, newWidth);
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get()
                    .onPanelContainerUpdated(
                            mNativeSidePanelCoordinatorAndroid, oldWidth, newWidth);
        }
    }

    /** Called when the side panel content has been replaced. */
    void onPanelContentReplaced() {
        log(TAG, "onPanelContentReplaced");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get()
                    .onPanelContentReplaced(mNativeSidePanelCoordinatorAndroid);
        }
    }

    /**
     * Called when the active status of this bridge is changed.
     *
     * <p>The active status of the bridge and its underlying native object is in sync with the
     * corresponding {@code TabModel}.
     *
     * <p>The active status will change when the user switches between standard and incognito {@code
     * TabModel}s in one {@code ChromeActivity}.
     *
     * <p>The active status will <i>not</i> change when another window gains focus, i.e., the bridge
     * being active only means the corresponding {@code TabModel} is the current {@code TabModel} in
     * the {@code ChromeActivity}.
     *
     * <p>This method will only be called in a multi-Profile {@code ChromeActivity}. For a
     * single-Profile {@code ChromeActivity}, there is only one bridge and it's always active.
     *
     * @param active Whether this bridge is active.
     */
    void onActiveChanged(boolean active) {
        log(TAG, "onActiveChanged");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get()
                    .onActiveChanged(mNativeSidePanelCoordinatorAndroid, active);
        }
    }

    /** Requests to close the side panel. */
    void closePanel(boolean suppressAnimations) {
        log(TAG, "closePanel");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get()
                    .closePanel(mNativeSidePanelCoordinatorAndroid, suppressAnimations);
        }
    }

    /**
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#onWillAutoClose()
     */
    void onWillAutoClose() {
        log(TAG, "onWillAutoClose");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get()
                    .onWillAutoClose(mNativeSidePanelCoordinatorAndroid);
        }
    }

    /**
     * @see org.chromium.chrome.browser.ui.side_ui.SideUiContainer#onWillAutoRestore()
     */
    void onWillAutoRestore() {
        log(TAG, "onWillAutoRestore");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get()
                    .onWillAutoRestore(mNativeSidePanelCoordinatorAndroid);
        }
    }

    private void createNativePtr(long nativeBrowserWindowPtr) {
        log(TAG, "createNativePtr", nativeBrowserWindowPtr);
        assert nativeBrowserWindowPtr != 0
                : "Native BrowserWindowInterface pointer shouldn't be null. Is the"
                        + " ChromeAndroidTaskFeatureKey correct?";
        assert mNativeSidePanelCoordinatorAndroid == 0
                : "Native SidePanelCoordinatorAndroid already exists";
        mNativeSidePanelCoordinatorAndroid =
                SidePanelCoordinatorAndroidBridgeJni.get().create(this, nativeBrowserWindowPtr);
    }

    private void destroyNativePtr() {
        log(TAG, "destroyNativePtr");
        if (mNativeSidePanelCoordinatorAndroid != 0) {
            SidePanelCoordinatorAndroidBridgeJni.get().destroy(mNativeSidePanelCoordinatorAndroid);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        log(TAG, "clearNativePtr");
        mNativeSidePanelCoordinatorAndroid = 0;
    }

    @CalledByNative
    private boolean canShow(@JniType("Profile*") Profile profile) {
        return mNativeBridgeSelector.canShow(profile);
    }

    @CalledByNative
    private void startOpeningPanel(
            @JniType("Profile*") Profile profile,
            View sidePanelNativeView,
            @JniType("std::u16string_view") String title,
            boolean shouldShowHeader,
            boolean suppressAnimations) {
        log(TAG, "startOpeningPanel", profile, sidePanelNativeView, title);
        mNativeBridgeSelector.startOpeningPanel(
                profile,
                new SidePanelContent(sidePanelNativeView, title, shouldShowHeader),
                suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNative
    private void startClosingPanel(
            @JniType("Profile*") Profile profile, boolean suppressAnimations) {
        log(TAG, "startClosingPanel", profile, suppressAnimations);
        mNativeBridgeSelector.startClosingPanel(
                profile, suppressAnimations || mDisableAnimationsForTesting);
    }

    @CalledByNative
    private void startReplacingPanelContent(
            @JniType("Profile*") Profile profile,
            View sidePanelNativeView,
            @JniType("std::u16string_view") @Nullable String title,
            boolean shouldShowHeader) {
        log(TAG, "startReplacingPanelContent", profile, sidePanelNativeView, title);
        mNativeBridgeSelector.startReplacingPanelContent(
                profile, new SidePanelContent(sidePanelNativeView, title, shouldShowHeader));
    }

    @CalledByNative
    private void endAnimations(@JniType("Profile*") Profile profile) {
        log(TAG, "endAnimations", profile);
        mNativeBridgeSelector.endAnimations(profile);
    }

    @CalledByNative
    private void completePendingContentReplacement(@JniType("Profile*") Profile profile) {
        log(TAG, "completePendingContentReplacement", profile);
        mNativeBridgeSelector.completePendingContentReplacement(profile);
    }

    @CalledByNativeForTesting
    private void configDeferredViewReplacementForTesting(
            @JniType("Profile*") Profile profile, boolean enable) {
        log(TAG, "configDeferredViewReplacementForTesting", profile, enable);
        mNativeBridgeSelector.configDeferredViewReplacementForTesting(profile, enable); // IN-TEST
    }

    @CalledByNativeForTesting
    private void simulateAutoCloseConditionForTesting(@JniType("Profile*") Profile profile) {
        log(TAG, "simulateAutoCloseConditionForTesting", profile);
        mNativeBridgeSelector.simulateAutoCloseConditionForTesting(profile); // IN-TEST
    }

    @CalledByNativeForTesting
    private void simulateAutoRestoreConditionForTesting(@JniType("Profile*") Profile profile) {
        log(TAG, "simulateAutoRestoreConditionForTesting", profile);
        mNativeBridgeSelector.simulateAutoRestoreConditionForTesting(profile); // IN-TEST
    }

    @CalledByNativeForTesting
    private void disableAnimationsForTesting() {
        log(TAG, "disableAnimationsForTesting");
        mDisableAnimationsForTesting = true;
    }

    @CalledByNativeForTesting
    private int getContainerWidthForTesting(@JniType("Profile*") Profile profile) {
        View view = mNativeBridgeSelector.getView(profile); // IN-TEST
        if (view == null || !view.isAttachedToWindow()) {
            return 0;
        }
        return view.getWidth();
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
        long create(SidePanelCoordinatorAndroidBridge caller, long nativeBrowserWindowPtr);

        /**
         * Destroys the native {@code SidePanelCoordinatorAndroid}.
         *
         * @param nativeSidePanelCoordinatorAndroid The address of the native {@code
         *     SidePanelCoordinatorAndroid}.
         */
        void destroy(long nativeSidePanelCoordinatorAndroid);

        /** See {@link SidePanelCoordinatorAndroidBridge#closePanel}. */
        void closePanel(long nativeSidePanelCoordinatorAndroid, boolean suppressAnimations);

        /** See {@link SidePanelCoordinatorAndroidBridge#init}. */
        void init(long nativeSidePanelCoordinatorAndroid);

        /** See {@link SidePanelCoordinatorAndroidBridge#hasContentToShow}. */
        boolean hasContentToShow(
                long nativeSidePanelCoordinatorAndroid, @JniType("TabAndroid*") Tab tab);

        /** See {@link SidePanelCoordinatorAndroidBridge#onPanelContainerUpdated}. */
        void onPanelContainerUpdated(
                long nativeSidePanelCoordinatorAndroid, int oldWidth, int newWidth);

        /** See {@link SidePanelCoordinatorAndroidBridge#onPanelContentReplaced}. */
        void onPanelContentReplaced(long nativeSidePanelCoordinatorAndroid);

        /** See {@link SidePanelCoordinatorAndroidBridge#onActiveChanged}. */
        void onActiveChanged(long nativeSidePanelCoordinatorAndroid, boolean active);

        /** See {@link SidePanelCoordinatorAndroidBridge#onWillAutoClose}. */
        void onWillAutoClose(long nativeSidePanelCoordinatorAndroid);

        /** See {@link SidePanelCoordinatorAndroidBridge#onWillAutoRestore}. */
        void onWillAutoRestore(long nativeSidePanelCoordinatorAndroid);
    }
}
