// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.util.TokenHolder;

/**
 * Handle browser controls lock feature that's relevant on large screen. Internally, this class uses
 * a token holder to represents when it should update the lock status. This class is intended to be
 * used for large form factors only.
 */
@NullMarked
public class TopControlsLockCoordinator {

    private final Context mContext;
    private final TopControlsStacker mTopControlsStacker;
    private final NullableObservableSupplier<@StripVisibilityState Integer>
            mTabStripVisibilitySupplier;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;

    // Token jar that represent the lock state should be paused.
    private final TokenHolder mDeferredTokens = new TokenHolder(this::updateLock);
    private final Callback<@Nullable @StripVisibilityState Integer> mStripVisibilityUpdateCallback =
            (vis) -> updateLock();
    private final AppHeaderObserver mAppHeaderObserver =
            new AppHeaderObserver() {
                @Override
                public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
                    updateLock();
                }
            };

    /**
     * Create the coordinate to manage TopControlsStacker locking mechanism.
     *
     * @param context The context used for current activity. Used to determine device form factor.
     * @param topControlsStacker The TopControlsStacker instance.
     * @param tabStripVisibilitySupplier The supplier of tab strip visibility.
     * @param desktopWindowStateManager The desktop windowing mode manager instance.
     */
    public TopControlsLockCoordinator(
            Context context,
            TopControlsStacker topControlsStacker,
            NullableObservableSupplier<@StripVisibilityState Integer> tabStripVisibilitySupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        mContext = context;
        mTopControlsStacker = topControlsStacker;
        mTabStripVisibilitySupplier = tabStripVisibilitySupplier;
        mDesktopWindowStateManager = desktopWindowStateManager;

        mTabStripVisibilitySupplier.addSyncObserverAndPostIfNonNull(mStripVisibilityUpdateCallback);
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(mAppHeaderObserver);
        }

        updateLock();
    }

    /** Get the token holder used to block scrolling updates. */
    public TokenHolder getDeferredLockingTokenJar() {
        return mDeferredTokens;
    }

    /** Destroy the instance and remove observation. */
    public void destroy() {
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(mAppHeaderObserver);
        }
        mTabStripVisibilitySupplier.removeObserver(mStripVisibilityUpdateCallback);
    }

    // Update the lock status when the deferred token jar has is empty.
    private void updateLock() {
        if (mDeferredTokens.hasTokens()) return;

        boolean needUpdates = mTopControlsStacker.setScrollingDisabled(shouldLockTopControls());
        if (needUpdates) {
            mTopControlsStacker.requestLayerUpdatePost(/* requireAnimate= */ false);
        }
    }

    // Core logic for this class.
    // The scrolling is disabled in the following scenario:
    // 1. When the device is in desktop windowing mode
    // 2. When the device is a large-tablet, and it's not hidden by height transition.
    private boolean shouldLockTopControls() {
        // Desktop form factor always take priority.
        // TODO(crbug.com/450970998): Explore if we can set this for all large tablets.
        if (DeviceInfo.isDesktop()) return true;

        // Enable lock in desktop window mode. Only relevant when the device supports it.
        if (mDesktopWindowStateManager != null) {
            var appHeaderState = mDesktopWindowStateManager.getAppHeaderState();
            if (appHeaderState != null && appHeaderState.isInDesktopWindow()) {
                return true;
            }
        }

        // If the tab strip is not visible, do not enable locking.
        @StripVisibilityState Integer visibility = mTabStripVisibilitySupplier.get();
        boolean tabStripIsHidden =
                visibility != null
                        && (visibility & StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION) != 0;
        if (tabStripIsHidden) {
            return false;
        }

        // If all the check passes, enable the condition based on form factors.
        return DeviceFormFactor.isNonMultiDisplayContextOnLargeTablet(mContext);
    }
}
