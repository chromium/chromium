// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * This class owns the model that hosts the contents of toolbar action context menu.
 *
 * <p>This class implements {@link Destroyable} to ensure proper cleanup of its associated native
 * resources. The native counterpart's lifetime is tied to this Java object; when {@link #destroy()}
 * is called, the native object is also destroyed.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionActionContextMenuBridge implements Destroyable {
    @JniType("::extensions::ExtensionActionContextMenuBridge")
    public interface NativeExtensionActionContextMenuBridge extends JniTypeToken {}

    private final JniUniquePtr<NativeExtensionActionContextMenuBridge> mNative;
    private boolean mDestroyed;

    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public ExtensionActionContextMenuBridge(
            ChromeAndroidTask task,
            Profile profile,
            String actionId,
            WebContents webContents,
            @ContextMenuSource int contextMenuSource) {
        mNative =
                ExtensionActionContextMenuBridgeJni.get()
                        .init(
                                task.getOrCreateNativeBrowserWindow(profile),
                                actionId,
                                webContents,
                                contextMenuSource);
    }

    /** Returns the {@link ModelList} of the content of the toolbar action's context menu. */
    public ModelList getModelList() {
        MenuModelBridge menuModelBridge =
                ExtensionActionContextMenuBridgeJni.get().getMenuModelBridge(mNative);
        return menuModelBridge.populateModelList();
    }

    /**
     * Cleans up the resources associated with this popup.
     *
     * <p>This method must be called when the {@link MenuModel} is no longer needed.
     */
    @Override
    public void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        mNative.destroy();
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    @NativeMethods
    public interface Native {
        JniUniquePtr<NativeExtensionActionContextMenuBridge> init(
                JniPtr<NativeBrowserWindowInterface> browserWindow,
                @JniType("std::string") String actionId,
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("ExtensionContextMenuModel::ContextMenuSource") int contextMenuSource);

        MenuModelBridge getMenuModelBridge(JniPtr<NativeExtensionActionContextMenuBridge> self);
    }
}
