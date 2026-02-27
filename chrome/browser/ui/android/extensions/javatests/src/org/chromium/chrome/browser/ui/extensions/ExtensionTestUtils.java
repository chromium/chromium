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
     * Helper to create a {@link ExtensionsMenuTypes.MenuEntryState} for a simple extension without
     * host permissions. The entry will only have a disabled action button.
     */
    public static ExtensionsMenuTypes.MenuEntryState createSimpleMenuEntry(
            String extensionId,
            String extensionName,
            @Nullable Bitmap extensionIcon,
            boolean isPinned) {
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
        return new ExtensionsMenuTypes.MenuEntryState(extensionId, actionButton, contextMenuButton);
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

        void disableExtension(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);

        void uninstallExtension(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);

        void setExtensionActionVisible(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String extensionId,
                boolean visible);

        int getRenderFrameHostCount(
                @JniType("Profile*") Profile profile, @JniType("std::string") String extensionId);
    }
}
