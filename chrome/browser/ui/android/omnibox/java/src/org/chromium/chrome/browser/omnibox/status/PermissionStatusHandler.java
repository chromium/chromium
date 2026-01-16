// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SiteSettingsUtil;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.permissions.PermissionUtil;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Handler for the Permission icon in the Status view. */
@NullMarked
public class PermissionStatusHandler implements PermissionDialogController.Observer {
    public static final int PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS = 8500;

    private final Context mContext;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final PermissionDialogController mPermissionDialogController;
    private final PageInfoIphController mPageInfoIphController;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Delegate mStatusDelegate;
    private final Handler mHandler;

    private @ContentSettingsType.EnumType int mLastPermission = ContentSettingsType.DEFAULT;
    private boolean mIsQuietClapperUi;
    private @Nullable WebContents mWebContents;
    private @Nullable Runnable mOnIconShownCallbackForTesting;
    private @Nullable Runnable mOnIconDismissedCallbackForTesting;
    private @Nullable Runnable mFinishIconAnimationRunnable;
    private @Nullable Runnable mShowClapperQuietIconRunnable;
    private @Nullable Runnable mTabSwitchCallbackForTesting;

    /** Delegate for {@link PermissionStatusHandler} to update the UI. */
    interface Delegate {
        /** Resets the state of the status icons, clearing them from display. */
        void resetCustomIconsStatus();

        /**
         * Displays a permission-related icon in the omnibox.
         *
         * @param icon The {@link PermissionIconResource} to display.
         */
        void showPermissionIcon(PermissionIconResource icon);

        /**
         * Updates the main location bar icon, potentially with a transition.
         *
         * @param transitionType The {@link IconTransitionType} to apply.
         */
        void updateLocationBarIcon(@IconTransitionType int transitionType);
    }

    /**
     * Constructs a new handler for managing permission status icons in the omnibox.
     *
     * @param context The {@link Context} for this Status component.
     * @param locationBarDataProvider Provides data to the location bar.
     * @param permissionDialogController Controls showing permission dialogs.
     * @param pageInfoIphController Manages when an IPH bubble for PageInfo is shown.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param statusDelegate Delegate to update the UI.
     * @param handler Handler for posting UI tasks.
     */
    PermissionStatusHandler(
            Context context,
            LocationBarDataProvider locationBarDataProvider,
            PermissionDialogController permissionDialogController,
            PageInfoIphController pageInfoIphController,
            MonotonicObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            Delegate statusDelegate,
            Handler handler) {
        mContext = context;
        mLocationBarDataProvider = locationBarDataProvider;
        mPermissionDialogController = permissionDialogController;
        mPageInfoIphController = pageInfoIphController;
        mProfileSupplier = profileSupplier;
        mWindowAndroid = windowAndroid;
        mStatusDelegate = statusDelegate;
        mHandler = handler;

        mPermissionDialogController.addObserver(this);
    }

    /**
     * Cleans up resources and removes observers. Should be called when the status mediator is
     * destroyed to prevent memory leaks.
     */
    void destroy() {
        mPermissionDialogController.removeObserver(this);
    }

    @Override
    public void onDialogResult(
            WindowAndroid window,
            @ContentSettingsType.EnumType int[] permissions,
            @ContentSetting int result) {
        // TODO(crbug.org/414527270) Investigate and potentially remove this check.
        if (window != mWindowAndroid) {
            return;
        }
        @ContentSettingsType.EnumType
        int permission = SiteSettingsUtil.getHighestPriorityPermission(permissions);
        // The permission is not available in the settings page. Do not show an icon.
        if (permission == ContentSettingsType.DEFAULT) return;

        showPermissionIcon(permission, result);
    }

    @Override
    public void showPermissionClapperQuietIcon(WindowAndroid window) {
        // Post the UI update to the message queue. This is critical for the Tab Switcher
        // transition.
        // When returning to a tab with a pending Clapper request, the native side calls this method
        // relatively early in the process. However, the Java UI triggers a couple of status icon
        // updates, which clear the local UI state. If we show the icon synchronously, this
        // subsequent reset wipes out the freshly created icon.
        // By posting, we allow the icon to be shown after the StatusMediator has finished its
        // reset/transition logic in most cases.
        mShowClapperQuietIconRunnable =
                () -> {
                    // PermissionDialogController, calling this method, is a singleton that survives
                    // tabswitches and notifies all observers. We must ensure this event is meant
                    // for the window owned by this handler.
                    Tab tab = mLocationBarDataProvider.getTab();
                    if (window != mWindowAndroid
                            || tab == null
                            || tab.getWebContents() == null
                            || tab.getWebContents().isDestroyed()) {
                        return;
                    }
                    showPermissionIcon(ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK);
                    mIsQuietClapperUi = true;

                    if (tab != null) {
                        mWebContents = tab.getWebContents();
                    }
                };
        mHandler.post(mShowClapperQuietIconRunnable);
    }

    @Override
    public void dismissPermissionClapperQuietIcon(WindowAndroid window) {
        if (window != mWindowAndroid) return;

        if (mShowClapperQuietIconRunnable != null) {
            mHandler.removeCallbacks(mShowClapperQuietIconRunnable);
            mShowClapperQuietIconRunnable = null;
        }
        reset(/* shouldDismissNativePrompt= */ false);
        mStatusDelegate.updateLocationBarIcon(IconTransitionType.ROTATE);
    }

    private void showPermissionIcon(
            @ContentSettingsType.EnumType int permission, @ContentSetting int contentSetting) {
        mStatusDelegate.resetCustomIconsStatus();

        mLastPermission = permission;

        boolean isIncognitoBranded = mLocationBarDataProvider.isIncognitoBranded();
        Drawable permissionDrawable =
                ContentSettingsResources.getIconForOmnibox(
                        mContext, mLastPermission, contentSetting, isIncognitoBranded);

        @StringRes
        int accessibilityDescriptionRes =
                ContentSettingsResources.getPermissionResultAnnouncementForScreenReader(
                        mLastPermission, contentSetting);

        PermissionIconResource permissionIconResource =
                new PermissionIconResource(
                        permissionDrawable, isIncognitoBranded, accessibilityDescriptionRes);
        permissionIconResource.setTransitionType(IconTransitionType.ROTATE);

        // We only want to notify the IPH controller after the icon transition is finished.
        // IPH is controlled by the FeatureEngagement system through finch with a field trial
        // testing configuration.
        permissionIconResource.setAnimationFinishedCallback(this::startIph);
        // Set the timer to switch the icon back afterwards.
        mHandler.removeCallbacksAndMessages(null);

        mStatusDelegate.showPermissionIcon(permissionIconResource);

        if (mOnIconShownCallbackForTesting != null) {
            mOnIconShownCallbackForTesting.run();
        }

        mFinishIconAnimationRunnable =
                () -> {
                    mFinishIconAnimationRunnable = null;
                    if (mIsQuietClapperUi && mWebContents != null && !mWebContents.isDestroyed()) {
                        PermissionUtil.notifyQuietIconDismissed(mWebContents);
                    }
                    reset(/* shouldDismissNativePrompt= */ true);
                    mStatusDelegate.updateLocationBarIcon(IconTransitionType.ROTATE);
                    if (mOnIconDismissedCallbackForTesting != null) {
                        mOnIconDismissedCallbackForTesting.run();
                    }
                };

        mHandler.postDelayed(
                mFinishIconAnimationRunnable, PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS);
    }

    private void startIph() {
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;
        mPageInfoIphController.onPermissionDialogShown(profile, getIphTimeoutMs());
    }

    /**
     * @return A timeout for the IPH bubble. The bubble is shown after the status icon animation
     *     finishes and should disappear when it animates out.
     */
    public int getIphTimeoutMs() {
        return PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS
                - (2 * StatusView.ICON_ROTATION_DURATION_MS);
    }

    /**
     * Returns the last permission type handled by this class. This is primarily used for testing to
     * verify that the correct permission logic was triggered.
     */
    public int getLastPermissionForTest() {
        return mLastPermission;
    }

    /**
     * Sets a callback to be run when the permission icon is shown.
     *
     * @param callback The callback to run.
     */
    public void setOnIconShownCallbackForTesting(@Nullable Runnable callback) {
        mOnIconShownCallbackForTesting = callback;
    }

    /**
     * Sets a callback to be run when the permission icon is dismissed.
     *
     * @param callback The callback to run.
     */
    public void setOnIconDismissedCallbackForTesting(@Nullable Runnable callback) {
        mOnIconDismissedCallbackForTesting = callback;
    }

    /**
     * Sets a callback to be run when a tab switch happened.
     *
     * @param callback The callback to run.
     */
    public void setTabSwitchCallbackForTesting(@Nullable Runnable callback) {
        mTabSwitchCallbackForTesting = callback;
    }

    /**
     * Triggers the permission icon timeout immediately for testing purposes.
     *
     * <p>This simulates the timeout expiring and executes the dismissal logic.
     */
    public void triggerIconTimeoutForTesting() {
        if (mFinishIconAnimationRunnable != null) {
            mHandler.removeCallbacks(mFinishIconAnimationRunnable);
            mFinishIconAnimationRunnable.run();
        }
    }

    /**
     * Returns whether the permission icon timeout is currently running.
     *
     * @return True if the timeout is running, false otherwise.
     */
    public boolean isIconTimeoutRunningForTesting() {
        return mFinishIconAnimationRunnable != null;
    }

    /**
     * Returns the configuration required to open the PageInfo dialog with the appropriate
     * permission highlighted. This is used when the user clicks on the permission icon in the
     * omnibox, ensuring they are taken directly to the relevant permission setting within PageInfo.
     */
    @Nullable ChromePageInfoHighlight getPageInfoHighlight() {
        if (mLastPermission != PageInfoController.NO_HIGHLIGHTED_PERMISSION) {
            if (mIsQuietClapperUi) {
                return ChromePageInfoHighlight.openPermissionSubpage(mLastPermission);
            }
            return ChromePageInfoHighlight.highlightPermission(mLastPermission);
        }
        return null;
    }

    /**
     * Resets the internal state of the handler, clearing the permission icon UI state. If this is
     * called with shouldDismissNativePrompt=true, it also dismisses the clapper quiet prompt (if
     * one is active) by notifying the native side.
     *
     * @param shouldDismissNativePrompt True if we should attempt to dismiss the native prompt (e.g.
     *     user interference). False if the prompt is being handled externally (e.g. tab switch or
     *     native dismissal).
     */
    void reset(boolean shouldDismissNativePrompt) {
        if (mFinishIconAnimationRunnable != null) {
            mHandler.removeCallbacks(mFinishIconAnimationRunnable);
            mFinishIconAnimationRunnable = null;

            if (shouldDismissNativePrompt && canDismissQuietClapperPrompt()) {
                PermissionUtil.notifyQuietIconDismissed(mWebContents);
                if (mOnIconDismissedCallbackForTesting != null) {
                    mOnIconDismissedCallbackForTesting.run();
                }
            }
        }
        if (!shouldDismissNativePrompt && mTabSwitchCallbackForTesting != null) {
            mTabSwitchCallbackForTesting.run();
        }

        clearState();
    }

    private void clearState() {
        mLastPermission = ContentSettingsType.DEFAULT;
        mIsQuietClapperUi = false;
        mWebContents = null;
    }

    @EnsuresNonNullIf("mWebContents")
    private boolean canDismissQuietClapperPrompt() {
        Tab currentTab = mLocationBarDataProvider.getTab();
        WebContents currentWebContents = currentTab != null ? currentTab.getWebContents() : null;

        return mIsQuietClapperUi
                && currentTab != null
                && currentWebContents == mWebContents
                && mWebContents != null
                && !mWebContents.isDestroyed();
    }

    /**
     * Resets the internal state when PageInfo is opened.
     *
     * <p>This clears the local UI state and if active the running auto-ignore timer.
     */
    void onPageInfoOpened() {
        clearState();
        if (mFinishIconAnimationRunnable != null) {
            mHandler.removeCallbacks(mFinishIconAnimationRunnable);
            mFinishIconAnimationRunnable = null;
        }
    }

    boolean isClapperQuietIconShowing() {
        return mIsQuietClapperUi;
    }
}
