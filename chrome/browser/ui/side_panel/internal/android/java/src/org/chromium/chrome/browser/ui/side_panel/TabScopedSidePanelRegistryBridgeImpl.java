// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implements {@link TabScopedSidePanelRegistryBridge}.
 *
 * <p>{@link TabScopedSidePanelRegistryBridge} is a {@link
 * org.chromium.chrome.browser.tab.TabObserver}. This class extends {@link EmptyTabObserver} to only
 * implement {@code TabObserver} methods we care about.
 */
@NullMarked
final class TabScopedSidePanelRegistryBridgeImpl extends EmptyTabObserver
        implements TabScopedSidePanelRegistryBridge {
    private long mNativeTabScopedSidePanelRegistryBridge;

    TabScopedSidePanelRegistryBridgeImpl(Tab tab) {
        createNativePtr(tab);
    }

    @VisibleForTesting
    TabScopedSidePanelRegistryBridgeImpl() {}

    @Override
    public void onDestroyed(Tab tab) {
        destroyNativePtr();
        tab.removeObserver(this);
    }

    @VisibleForTesting
    void destroyNativePtr() {
        if (mNativeTabScopedSidePanelRegistryBridge != 0) {
            TabScopedSidePanelRegistryBridgeImplJni.get()
                    .destroy(mNativeTabScopedSidePanelRegistryBridge);
        }
    }

    long createNativePtrForTesting(long nativeMockTabInterfacePtr) {
        assert mNativeTabScopedSidePanelRegistryBridge == 0
                : "Native TabScopedSidePanelRegistry already exists";

        mNativeTabScopedSidePanelRegistryBridge =
                TabScopedSidePanelRegistryBridgeImplJni.get()
                        .createForTesting(this, nativeMockTabInterfacePtr); // IN-TEST
        return mNativeTabScopedSidePanelRegistryBridge;
    }

    long getNativePtrForTesting() {
        return mNativeTabScopedSidePanelRegistryBridge;
    }

    private void createNativePtr(Tab tab) {
        assert mNativeTabScopedSidePanelRegistryBridge == 0
                : "Native TabScopedSidePanelRegistry already exists";

        mNativeTabScopedSidePanelRegistryBridge =
                TabScopedSidePanelRegistryBridgeImplJni.get().create(this, tab);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTabScopedSidePanelRegistryBridge = 0;
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code SidePanelRegistry} that's Tab-scoped.
         *
         * @param caller The Java object calling this method.
         * @param tab The {@link Tab} that the native {@code SidePanelRegistry} is associated with.
         * @return The address of the native {@code SidePanelRegistry}.
         */
        long create(TabScopedSidePanelRegistryBridgeImpl caller, Tab tab);

        /**
         * Destroys the native {@code SidePanelRegistry}.
         *
         * @param nativeTabScopedSidePanelRegistryBridge The address of the native {@code
         *     SidePanelRegistry}.
         */
        void destroy(long nativeTabScopedSidePanelRegistryBridge);

        /**
         * Creates a native {@code SidePanelRegistry} that's Tab-scoped for native tests.
         *
         * @param caller The Java object calling this method.
         * @param nativeMockTabInterfacePtr The pointer to a native {@code MockTabInterface}.
         * @return The address of the native {@code SidePanelRegistry}.
         */
        long createForTesting( // IN-TEST
                TabScopedSidePanelRegistryBridgeImpl caller, long nativeMockTabInterfacePtr);
    }
}
