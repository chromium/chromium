// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.component_updater;

import android.os.ParcelFileDescriptor;

import androidx.annotation.IntDef;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Map;

/**
 * Provides JNI bridge to the native ComponentLoaderPolicy.
 */
@JNINamespace("component_updater")
public class ComponentLoaderPolicyBridge {
    private static final long NATIVE_NULL = 0;

    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private long mNativeAndroidComponentLoaderPolicy = NATIVE_NULL;

    // These values maybe persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. They have to be kept in synce with the native enum
    // class component_updater::ComponentLoadError.
    @IntDef({
            ComponentLoadError.FAILED_TO_CONNECT_TO_COMPONENTS_PROVIDER_SERVICE,
            ComponentLoadError.REMOTE_EXCEPTION,
            ComponentLoadError.COMPONENTS_PROVIDER_SERVICE_ERROR,
            ComponentLoadError.MISSING_MANIFEST,
            ComponentLoadError.MALFORMED_MANIFEST,
            ComponentLoadError.INVALID_VERSION,
    })
    /* package */ static @interface ComponentLoadError {
        int FAILED_TO_CONNECT_TO_COMPONENTS_PROVIDER_SERVICE = 0;
        int REMOTE_EXCEPTION = 1;
        int COMPONENTS_PROVIDER_SERVICE_ERROR = 2;
        int MISSING_MANIFEST = 3;
        int MALFORMED_MANIFEST = 4;
        int INVALID_VERSION = 5;
    }

    @CalledByNative
    private ComponentLoaderPolicyBridge(long nativeAndroidComponentLoaderPolicy) {
        mNativeAndroidComponentLoaderPolicy = nativeAndroidComponentLoaderPolicy;
    }

    /**
     * ComponentLoaded is called when the loader successfully gets file descriptors for all files
     * in the component from the ComponentsProviderService.
     *
     * Should close all file descriptors after using them. Can be called on a background thread.
     *
     * Exactly one of componentLoaded or componentLoadFailed should be called exactly once.
     *
     * @param fileMap maps file relative paths in the install directory to its file descriptor.
     */
    public void componentLoaded(Map<String, ParcelFileDescriptor> fileMap) {
        ThreadUtils.assertOnUiThread();
        assert mNativeAndroidComponentLoaderPolicy != NATIVE_NULL;

        // Flatten the map into two arrays one for keys and another for values to be able to
        // pass them to native.
        String[] fileNames = new String[fileMap.size()];
        int[] fds = new int[fileMap.size()];
        int i = 0;
        for (Map.Entry<String, ParcelFileDescriptor> file : fileMap.entrySet()) {
            fileNames[i] = file.getKey();
            fds[i] = file.getValue().detachFd();
            ++i;
        }
        ComponentLoaderPolicyBridgeJni.get().componentLoaded(
                mNativeAndroidComponentLoaderPolicy, fileNames, fds);
        // Setting it to null, because it is deleted after componentLoaded is called.
        mNativeAndroidComponentLoaderPolicy = NATIVE_NULL;

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /**
     * Called if connection to the service fails, components files are not found or if the manifest
     * file is missing or invalid. Can be called on a background thread.
     *
     * Exactly one of componentLoaded or componentLoadFailed should be called exactly once.
     *
     * @param errorCode the code of the error that caused the failure.
     */
    public void componentLoadFailed(@ComponentLoadError int errorCode) {
        ThreadUtils.assertOnUiThread();
        assert mNativeAndroidComponentLoaderPolicy != NATIVE_NULL;

        ComponentLoaderPolicyBridgeJni.get().componentLoadFailed(
                mNativeAndroidComponentLoaderPolicy, errorCode);
        // Setting it to null, because it is deleted after componentLoadFailed is called.
        mNativeAndroidComponentLoaderPolicy = NATIVE_NULL;

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /**
     * Returns the component's unique id by parsing its SHA2 will be used to request components
     * files from the ComponentsProviderService. Can be called on a background thread.
     */
    public String getComponentId() {
        ThreadUtils.assertOnUiThread();
        assert mNativeAndroidComponentLoaderPolicy != NATIVE_NULL;

        return ComponentLoaderPolicyBridgeJni.get().getComponentId(
                mNativeAndroidComponentLoaderPolicy);
    }

    @CalledByNative
    private static ComponentLoaderPolicyBridge[] createNewArray(int size) {
        return new ComponentLoaderPolicyBridge[size];
    }

    @CalledByNative
    private static void setArrayElement(
            ComponentLoaderPolicyBridge[] array, int index, ComponentLoaderPolicyBridge policy) {
        array[index] = policy;
    }

    @NativeMethods
    interface Natives {
        void componentLoaded(
                long nativeAndroidComponentLoaderPolicy, String[] fileNames, int[] fds);
        void componentLoadFailed(long nativeAndroidComponentLoaderPolicy, int errorCode);
        String getComponentId(long nativeAndroidComponentLoaderPolicy);
    }
}
