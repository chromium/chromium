// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
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

    private long mNativeExtensionActionContextMenuBridge;

    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public ExtensionActionContextMenuBridge(
            ChromeAndroidTask task,
            String actionId,
            WebContents webContents,
            @ContextMenuSource int contextMenuSource) {
        mNativeExtensionActionContextMenuBridge =
                ExtensionActionContextMenuBridgeJni.get()
                        .init(
                                task.getOrCreateNativeBrowserWindowPtr(),
                                actionId,
                                webContents,
                                contextMenuSource);
    }

    /** Returns the {@link ModelList} of the content of the toolbar action's context menu. */
    public ModelList getModelList() {
        MenuModelBridge menuModelBridge =
                ExtensionActionContextMenuBridgeJni.get()
                        .getMenuModelBridge(mNativeExtensionActionContextMenuBridge);
        return menuModelBridge.populateModelList();
    }

    /**
     * Cleans up the resources associated with this popup.
     *
     * <p>This method must be called when the {@link MenuModel} is no longer needed.
     */
    @Override
    public void destroy() {
        if (mNativeExtensionActionContextMenuBridge != 0) {
            ExtensionActionContextMenuBridgeJni.get()
                    .destroy(mNativeExtensionActionContextMenuBridge);
            mNativeExtensionActionContextMenuBridge = 0;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    @NativeMethods
    public interface Native {
        long init(
                long androidBrowserWindowPtr,
                @JniType("std::string") String actionId,
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("ExtensionContextMenuModel::ContextMenuSource") int contextMenuSource);

        MenuModelBridge getMenuModelBridge(long nativeExtensionActionContextMenuBridge);

        void destroy(long nativeExtensionActionContextMenuBridge);
    }
}
