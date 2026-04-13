// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.io.File;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

/** Provides utilities for extension-related integration tests. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionTestUtils {
    /**
     * Loads an unpacked extension from a given directory, and returns a load extension's ID.
     *
     * <p>This function must be called from a non-UI thread since it needs to run some operations on
     * the UI thread, while others on other threads.
     *
     * @param profile The profile to load an extension for.
     * @param rootDir The root directory containing an unpacked extension.
     * @return The load extension's ID.
     */
    public static String loadUnpackedExtension(Profile profile, File rootDir) {
        ThreadUtils.assertOnBackgroundThread();

        CompletableFuture<String> future = new CompletableFuture<>();
        ThreadUtils.runOnUiThread(
                () -> {
                    ExtensionTestUtilsJni.get()
                            .loadUnpackedExtensionAsync(
                                    profile,
                                    rootDir.toString(),
                                    (extensionId) -> {
                                        future.complete(extensionId);
                                    });
                });

        try {
            return future.get();
        } catch (InterruptedException | ExecutionException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Enable the extension with the given ID.
     *
     * @param profile The profile to enable the extension for.
     * @param extensionId The ID of the extension to disable.
     */
    public static void enableExtension(Profile profile, String extensionId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExtensionTestUtilsJni.get().enableExtension(profile, extensionId);
                });
    }

    /**
     * Disables the extension with the given ID.
     *
     * @param profile The profile to disable the extension for.
     * @param extensionId The ID of the extension to disable.
     */
    public static void disableExtension(Profile profile, String extensionId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExtensionTestUtilsJni.get().disableExtension(profile, extensionId);
                });
    }

    /**
     * Uninstalls the extension with the given ID.
     *
     * @param profile The profile the extension belongs to.
     * @param extensionId The ID of the extension to uninstall.
     */
    public static void uninstallExtension(Profile profile, String extensionId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExtensionTestUtilsJni.get().uninstallExtension(profile, extensionId);
                });
    }

    /**
     * Sets whether the extension action is visible in the toolbar.
     *
     * @param profile The profile the extension belongs to.
     * @param extensionId The ID of the extension.
     * @param visible Whether the action should be visible.
     */
    public static void setExtensionActionVisible(
            Profile profile, String extensionId, boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExtensionTestUtilsJni.get()
                            .setExtensionActionVisible(profile, extensionId, visible);
                });
    }

    /**
     * Returns the list of pinned actions.
     *
     * @param profile The profile that the extension belongs to.
     * @return The list of IDs of pinned actions.
     */
    public static String[] getPinnedActionIds(Profile profile) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ExtensionTestUtilsJni.get().getPinnedActionIds(profile));
    }

    /**
     * Returns the number of active RenderFrameHosts for the given extension.
     *
     * @param profile The profile the extension belongs to.
     * @param extensionId The ID of the extension.
     * @return The number of active RenderFrameHosts.
     */
    public static int getRenderFrameHostCount(Profile profile, String extensionId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ExtensionTestUtilsJni.get()
                            .getRenderFrameHostCount(profile, extensionId);
                });
    }

    /**
     * Sets whether host permissions are withheld for the given extension.
     *
     * @param profile The profile the extension belongs to.
     * @param extensionId The ID of the extension.
     * @param withhold Whether to withhold host permissions.
     */
    public static void setWithholdHostPermissions(
            Profile profile, String extensionId, boolean withhold) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExtensionTestUtilsJni.get()
                            .setWithholdHostPermissions(profile, extensionId, withhold);
                });
    }

    /**
     * Adds an active host access request for the given extension on the specified web contents.
     *
     * @param profile The profile the extension belongs to.
     * @param webContents The web contents where the extension is requesting access.
     * @param extensionId The ID of the extension requesting access.
     */
    public static void addHostAccessRequest(
            Profile profile, WebContents webContents, String extensionId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExtensionTestUtilsJni.get()
                            .addHostAccessRequest(profile, webContents, extensionId);
                });
    }

    /**
     * Checks if the extension has been granted host permission for the given URL.
     *
     * @param profile The profile the extension belongs to.
     * @param extensionId The ID of the extension.
     * @param url The URL to check access for.
     * @return True if the extension has granted host permission for the URL, false otherwise.
     */
    public static boolean hasGrantedHostPermission(
            Profile profile, String extensionId, String url) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ExtensionTestUtilsJni.get()
                            .hasGrantedHostPermission(profile, extensionId, url);
                });
    }

    /**
     * Helper to create a {@link ExtensionsMenuTypes.ExtensionSitePermissionsState}. Default values
     * are set for the show requests toggle: status is set to ENABLED, text and accessible name are
     * empty, and it is toggled ON.
     */
    public static ExtensionsMenuTypes.ExtensionSitePermissionsState
            createExtensionSitePermissionsState(
                    String extensionName, @Nullable Bitmap extensionIcon) {
        ExtensionsMenuTypes.ControlState toggleState =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ true,
                        /* icon= */ null);
        return new ExtensionsMenuTypes.ExtensionSitePermissionsState(
                extensionName, extensionIcon, toggleState);
    }

    /**
     * Helper to create a {@link ExtensionsMenuTypes.MenuEntryState} for a simple extension without
     * host permissions. The entry will have a disabled action button, and hidden site access toggle
     * and site permissions button.
     */
    public static ExtensionsMenuTypes.MenuEntryState createSimpleMenuEntry(
            String extensionId,
            String extensionName,
            @Nullable Bitmap extensionIcon,
            boolean isPinned) {
        ExtensionsMenuTypes.ControlState siteAccessToggle =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        ExtensionsMenuTypes.ControlState sitePermissionsButton =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.HIDDEN,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        /* icon= */ null);
        return createMenuEntry(
                extensionId,
                extensionName,
                extensionIcon,
                isPinned,
                siteAccessToggle,
                sitePermissionsButton);
    }

    /**
     * Helper to create a {@link ExtensionsMenuTypes.MenuEntryState} for an extension with host
     * permissions. The entry will have enabled site permissions button and site access toggle,
     * defaulted to granted access information.
     */
    public static ExtensionsMenuTypes.MenuEntryState createMenuEntryWithHostPermissions(
            String extensionId,
            String extensionName,
            @Nullable Bitmap extensionIcon,
            boolean isPinned) {
        ExtensionsMenuTypes.ControlState siteAccessToggle =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "Allowed on this site",
                        /* isOn= */ true,
                        /* icon= */ null);
        ExtensionsMenuTypes.ControlState sitePermissionsButton =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "Always on all sites",
                        /* accessibleName= */ "Always on all sites. Select to change site"
                                + " permissions",
                        /* tooltipText= */ "Change site permissions",
                        /* isOn= */ false,
                        /* icon= */ null);
        return createMenuEntry(
                extensionId,
                extensionName,
                extensionIcon,
                isPinned,
                siteAccessToggle,
                sitePermissionsButton);
    }

    /** Helper to create a {@link ExtensionsMenuTypes.MenuEntryState}. */
    public static ExtensionsMenuTypes.MenuEntryState createMenuEntry(
            String extensionId,
            String extensionName,
            @Nullable Bitmap extensionIcon,
            boolean isPinned,
            ExtensionsMenuTypes.ControlState siteAccessToggle,
            ExtensionsMenuTypes.ControlState sitePermissionsButton) {
        ExtensionsMenuTypes.ControlState actionButton =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.DISABLED,
                        extensionName,
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ false,
                        extensionIcon);
        ExtensionsMenuTypes.ControlState contextMenuButton =
                new ExtensionsMenuTypes.ControlState(
                        ExtensionsMenuTypes.ControlState.Status.ENABLED,
                        /* text= */ "",
                        /* accessibleName= */ "",
                        /* tooltipText= */ "",
                        /* isOn= */ isPinned,
                        /* icon= */ null);
        return new ExtensionsMenuTypes.MenuEntryState(
                extensionId,
                actionButton,
                contextMenuButton,
                siteAccessToggle,
                sitePermissionsButton);
    }

    /** Helper to create a simple icon with the given color. */
    public static Bitmap createSimpleIcon(int color) {
        Bitmap bitmap = Bitmap.createBitmap(12, 12, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint();
        paint.setColor(color);
        canvas.drawRect(0, 0, 12, 12, paint);
        return bitmap;
    }

    @NativeMethods
    public interface Natives {
        void loadUnpackedExtensionAsync(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String rootDir,
                Callback<String> callback);

        void enableExtension(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);

        void disableExtension(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);

        void uninstallExtension(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);

        void setExtensionActionVisible(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String extensionId,
                boolean visible);

        @JniType("std::vector<std::string>")
        String[] getPinnedActionIds(@JniType("Profile*") Profile profile);

        int getRenderFrameHostCount(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);

        void setWithholdHostPermissions(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String extensionId,
                boolean withhold);

        void addHostAccessRequest(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("std::string") String extensionId);

        boolean hasGrantedHostPermission(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String extensionId,
                @JniType("std::string") String url);
    }
}
