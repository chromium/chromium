// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SiteSettingsUtil;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.ui.base.WindowAndroid;

/** Handler for the Permission icon in the Status view. */
@NullMarked
public class PermissionStatusHandler implements PermissionDialogController.Observer {
    public static final int PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS = 8500;

    private final Context mContext;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final PermissionDialogController mPermissionDialogController;
    private final PageInfoIphController mPageInfoIphController;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Delegate mStatusDelegate;
    private final Handler mHandler;

    private @ContentSettingsType.EnumType int mLastPermission = ContentSettingsType.DEFAULT;
    private @StringRes int mAccessibilityDescriptionRes;

    /** Delegate for {@link PermissionStatusHandler} to update the UI. */
    interface Delegate {
        /** Resets the state of the status icons, clearing them from display. */
        void resetCustomIconsStatus();

        /**
         * Displays a permission-related icon in the omnibox.
         *
         * @param icon The {@link PermissionIconResource} to display.
         * @param descriptionRes The accessibility description for the icon.
         */
        void showPermissionIcon(PermissionIconResource icon, @StringRes int descriptionRes);

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
     * @param urlBarEditingTextStateProvider Provides url bar text state.
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
            ObservableSupplier<Profile> profileSupplier,
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

        mStatusDelegate.resetCustomIconsStatus();

        mLastPermission = permission;

        boolean isIncognitoBranded = mLocationBarDataProvider.isIncognitoBranded();
        Drawable permissionDrawable =
                ContentSettingsResources.getIconForOmnibox(
                        mContext, mLastPermission, result, isIncognitoBranded);
        PermissionIconResource permissionIconResource =
                new PermissionIconResource(permissionDrawable, isIncognitoBranded);
        permissionIconResource.setTransitionType(IconTransitionType.ROTATE);

        // We only want to notify the IPH controller after the icon transition is finished.
        // IPH is controlled by the FeatureEngagement system through finch with a field trial
        // testing configuration.
        permissionIconResource.setAnimationFinishedCallback(this::startIph);
        // Set the timer to switch the icon back afterwards.
        mHandler.removeCallbacksAndMessages(null);

        mAccessibilityDescriptionRes =
                ContentSettingsResources.getPermissionResultAnnouncementForScreenReader(
                        mLastPermission, result);

        mStatusDelegate.showPermissionIcon(permissionIconResource, mAccessibilityDescriptionRes);

        Runnable finishIconAnimation =
                () -> {
                    mStatusDelegate.updateLocationBarIcon(IconTransitionType.ROTATE);
                };

        mHandler.postDelayed(finishIconAnimation, PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS);
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
     * Returns the configuration required to open the PageInfo dialog with the appropriate
     * permission highlighted. This is used when the user clicks on the permission icon in the
     * omnibox, ensuring they are taken directly to the relevant permission setting within PageInfo.
     */
    @Nullable ChromePageInfoHighlight getPageInfoHighlight() {
        if (mLastPermission != PageInfoController.NO_HIGHLIGHTED_PERMISSION) {
            return ChromePageInfoHighlight.forPermission(mLastPermission);
        }
        return null;
    }

    /**
     * Resets the internal state of the handler, clearing the last tracked permission. This ensures
     * that subsequent interactions (like opening PageInfo) do not erroneously use stale permission
     * data.
     */
    void reset() {
        mLastPermission = ContentSettingsType.DEFAULT;
    }

    /**
     * Returns the resource ID for the accessibility description string associated with the current
     * permission icon. This is used by screen readers to announce the permission status to the
     * user.
     */
    int getAccessibilityDescriptionRes() {
        return mAccessibilityDescriptionRes;
    }
}
