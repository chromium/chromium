// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.chromium.build.NullUtil.assertNonNull;

import android.graphics.Rect;
import android.util.ArrayMap;
import android.view.View;

import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.Map;

/**
 * Facilitates bidirectional communication between {@link SidePanelContainerCoordinatorImpl} and
 * side panel native bridges.
 *
 * <p>On Android:
 *
 * <ul>
 *   <li>{@code ChromeActivity} : {@link SidePanelContainerCoordinatorImpl} = (1:1)
 *   <li>{@code ChromeActivity} : {@link Profile} = (1:1) or (1:2)
 *   <li>[Native {@code BrowserWindowInterface}(BWI)] : {@link Profile} = (1:1)
 *   <li>[Native {@code SidePanelCoordinatorAndroid}] : BWI = (1:1)
 *   <li>[Native window-scoped {@code SidePanelRegistry}] : BWI = (1:1)
 * </ul>
 *
 * Therefore:
 *
 * <ul>
 *   <li>When {@link SidePanelContainerCoordinatorImpl} needs to call a native class, it needs to
 *       select the one with the current/active {@link Profile}.
 *   <li>When a native class needs to call {@link SidePanelContainerCoordinatorImpl}, the UI should
 *       only change if the native class' {@link Profile} is the current/active {@link Profile}.
 * </ul>
 */
@NullMarked
final class SidePanelNativeBridgeSelector {

    /**
     * Contains native bridges whose underlying native objects are scoped to a {@code
     * BrowserWindowInterface}.
     */
    private static final class NativeBridges {
        final SidePanelCoordinatorAndroidBridge mCoordinatorBridge;

        // TODO(crbug.com/540949995): Remove annotation once implementation is complete.
        @SuppressWarnings("UnusedVariable")
        final WindowScopedSidePanelRegistryBridge mWindowScopedRegistryBridge;

        NativeBridges(
                SidePanelCoordinatorAndroidBridge coordinatorBridge,
                WindowScopedSidePanelRegistryBridge windowScopedRegistryBridge) {
            mCoordinatorBridge = coordinatorBridge;
            mWindowScopedRegistryBridge = windowScopedRegistryBridge;
        }
    }

    private final ActivityWindowAndroid mWindowAndroid;
    private final SidePanelContainerCoordinatorImpl mSidePanelContainerCoordinator;
    private final TabModelSelector mTabModelSelector;
    private final Map<Profile, NativeBridges> mNativeBridges = new ArrayMap<>();

    SidePanelNativeBridgeSelector(
            ActivityWindowAndroid windowAndroid,
            SidePanelContainerCoordinatorImpl sidePanelContainerCoordinator,
            TabModelSelector tabModelSelector) {
        mWindowAndroid = windowAndroid;
        mSidePanelContainerCoordinator = sidePanelContainerCoordinator;
        mTabModelSelector = tabModelSelector;
    }

    /** Initializes native objects. */
    void init() {
        var activity = assertNonNull(mWindowAndroid.getActivity().get());
        int taskId = ApplicationStatus.getTaskId(activity);
        ChromeAndroidTask chromeAndroidTask =
                assertNonNull(ChromeAndroidTaskTrackerFactory.getInstance().get(taskId));
        var currentProfile = assertNonNull(mTabModelSelector.getCurrentModel().getProfile());

        // Instantiate native objects.
        //
        // Note: The lifecycles of SidePanelCoordinatorAndroid and the window-scoped
        // SidePanelRegistry are in sync with a native BrowserWindowInterface, but
        // SidePanelCoordinatorAndroid doesn't own the SidePanelRegistry, or vice versa.
        // This matches the WML implementation.
        var windowScopedRegistryBridge =
                (WindowScopedSidePanelRegistryBridge)
                        assertNonNull(
                                chromeAndroidTask.addFeature(
                                        new ChromeAndroidTaskFeatureKey(
                                                WindowScopedSidePanelRegistryBridge.class,
                                                currentProfile,
                                                mWindowAndroid),
                                        WindowScopedSidePanelRegistryBridge::new));
        var coordinatorBridge =
                (SidePanelCoordinatorAndroidBridge)
                        assertNonNull(
                                chromeAndroidTask.addFeature(
                                        new ChromeAndroidTaskFeatureKey(
                                                SidePanelCoordinatorAndroidBridge.class,
                                                currentProfile,
                                                mWindowAndroid),
                                        () -> new SidePanelCoordinatorAndroidBridge(this)));
        mNativeBridges.put(
                currentProfile, new NativeBridges(coordinatorBridge, windowScopedRegistryBridge));

        // Do initialization work for the current profile.
        coordinatorBridge.init();
    }

    /** Returns the {@link SidePanelCoordinatorAndroidBridge} for the current {@link Profile}. */
    @Nullable SidePanelCoordinatorAndroidBridge getCurrentCoordinatorBridge() {
        var profile = mTabModelSelector.getCurrentModel().getProfile();
        if (profile == null) {
            return null;
        }

        var nativeBridges = mNativeBridges.get(profile);
        return nativeBridges == null ? null : nativeBridges.mCoordinatorBridge;
    }

    /** Returns whether the side panel container can be shown for the given {@link Profile}. */
    boolean canShow(Profile profile) {
        return profile.equals(mTabModelSelector.getCurrentModel().getProfile())
                && mSidePanelContainerCoordinator.canShow();
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#startOpeningPanel}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void startOpeningPanel(
            Profile profile,
            SidePanelContent content,
            @Nullable Rect initialContentBounds,
            boolean suppressAnimations) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.startOpeningPanel(
                content, initialContentBounds, suppressAnimations);
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#startClosingPanel}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void startClosingPanel(Profile profile, boolean suppressAnimations) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.startClosingPanel(suppressAnimations);
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#startReplacingPanelContent}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void startReplacingPanelContent(Profile profile, SidePanelContent content) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.startReplacingPanelContent(content);
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#endAnimations}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void endAnimations(Profile profile) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.endAnimations();
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#completePendingContentReplacement}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void completePendingContentReplacement(Profile profile) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.completePendingContentReplacement();
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#configDeferredViewReplacementForTesting}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void configDeferredViewReplacementForTesting(Profile profile, boolean enable) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.configDeferredViewReplacementForTesting(enable); // IN-TEST
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#simulateAutoCloseConditionForTesting}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void simulateAutoCloseConditionForTesting(Profile profile) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.simulateAutoCloseConditionForTesting(); // IN-TEST
    }

    /**
     * See {@link SidePanelContainerCoordinatorImpl#simulateAutoRestoreConditionForTesting}.
     *
     * <p>The given {@link Profile} must be the current {@link Profile}.
     */
    void simulateAutoRestoreConditionForTesting(Profile profile) {
        assertCurrentProfile(profile);
        mSidePanelContainerCoordinator.simulateAutoRestoreConditionForTesting(); // IN-TEST
    }

    /** See {@link SidePanelContainerCoordinatorImpl#getView}. */
    @Nullable View getView(Profile profile) {
        return profile.equals(mTabModelSelector.getCurrentModel().getProfile())
                ? mSidePanelContainerCoordinator.getView()
                : null;
    }

    void destroy() {
        // We don't need to explicitly destroy the native objects as they are managed by
        // ChromeAndroidTask.
        // We only need to remove the Java bridges from the map.
        mNativeBridges.clear();
    }

    private void assertCurrentProfile(Profile profile) {
        assert profile.equals(mTabModelSelector.getCurrentModel().getProfile());
    }
}
