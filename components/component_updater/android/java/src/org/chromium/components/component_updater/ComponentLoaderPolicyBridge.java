// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.component_updater;

import android.os.ParcelFileDescriptor;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Map;

/**
 * Provides JNI bridge to the native ComponentLoaderPolicy.
 */
@JNINamespace("component_updater")
public class ComponentLoaderPolicyBridge {
    private final long mNativeComponentLoader;

    public ComponentLoaderPolicyBridge(long nativeComponentLoader) {
        mNativeComponentLoader = nativeComponentLoader;
    }

    /**
     * ComponentLoaded is called when the loader successfully gets file descriptors for all files
     * in the component from the ComponentsProviderService.
     *
     * Should close all file descriptors after using them. Can be called on a background thread.
     *
     * @param fileMap maps file relative paths in CRX file to its file descriptor.
     */
    public void componentLoaded(Map<String, ParcelFileDescriptor> fileMap) {
        // Flatten the map into two arrays one for keys and another for values to be able to
        // pass them to native.
        String[] fileNames = new String[fileMap.size()];
        int[] fds = new int[fileMap.size()];
        int i = 0;
        for (Map.Entry<String, ParcelFileDescriptor> file : fileMap.entrySet()) {
            fileNames[i] = file.getKey();
            fds[i] = file.getValue().getFd();
            ++i;
        }
        ComponentLoaderPolicyBridgeJni.get().componentLoaded(
                mNativeComponentLoader, fileNames, fds);
    }

    /**
     * Called if connection to the service fails, components files are not found or if the manifest
     * file is missing or invalid. Can be called on a background thread.
     */
    public void componentLoadFailed() {
        ComponentLoaderPolicyBridgeJni.get().componentLoadFailed(mNativeComponentLoader);
    }

    /**
     * Returns the component's unique id by parsing its SHA2 will be used to request components
     * files from the ComponentsProviderService. Can be called on a background thread.
     */
    public String getComponentId() {
        return ComponentLoaderPolicyBridgeJni.get().getComponentId(mNativeComponentLoader);
    }

    @NativeMethods
    interface Natives {
        void componentLoaded(long nativeEmbeddedComponentLoader, String[] fileNames, int[] fds);
        void componentLoadFailed(long nativeEmbeddedComponentLoader);
        String getComponentId(long nativeEmbeddedComponentLoader);
    }
}
