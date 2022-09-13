// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Facilitates injection of test dependencies for use in integration tests.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantDependencyInjector {
    /**
     * Interface for service providers.
     */
    public interface NativeServiceProvider {
        /**
         * Returns a pointer to a native service instance, or 0 if no service was created.
         */
        long createNativeService(long nativeClientAndroid);
    }

    /**
     * Interface for service request senders.
     */
    public interface NativeServiceRequestSenderProvider {
        /**
         * Returns a pointer to a native service request sender, or 0 on failure.
         */
        long createNativeServiceRequestSender();
    }

    /**
     * Interface for TTS controller providers.
     */
    public interface NativeTtsControllerProvider {
        /**
         * Returns a pointer to a native TTS controller, or 0 on failure.
         */
        long createNativeTtsController();
    }

    /**
     * Provider to create the native service to inject. Will be automatically called upon client
     * startup.
     */
    private static NativeServiceProvider sNativeServiceProvider;

    /**
     * Provider to create the native service request sender to inject. Will be automatically called
     * upon trigger script startup.
     */
    private static NativeServiceRequestSenderProvider sNativeServiceRequestSenderProvider;

    /**
     * Provider to create the native TTS controller to inject. Will be automatically called upon
     * startup.
     */
    private static NativeTtsControllerProvider sNativeTtsControllerProvider;

    /**
     * Sets a service provider to create a native service to inject upon client startup.
     */
    public static void setServiceToInject(NativeServiceProvider nativeServiceProvider) {
        sNativeServiceProvider = nativeServiceProvider;
    }

    /**
     * Sets a service request sender provider to create a native service request sender to inject
     * upon trigger script startup.
     */
    public static void setServiceRequestSenderToInject(
            NativeServiceRequestSenderProvider nativeServiceRequestSenderProvider) {
        sNativeServiceRequestSenderProvider = nativeServiceRequestSenderProvider;
    }

    /**
     * Sets a TTS controller provider to create a native TTS controller to inject upon startup.
     * @param nativeTtsControllerProvider
     */
    public static void setTtsControllerToInject(
            NativeTtsControllerProvider nativeTtsControllerProvider) {
        sNativeTtsControllerProvider = nativeTtsControllerProvider;
    }

    /**
     * Returns the native pointer to the service to inject, or 0 if no service has been set (and the
     * default should be used).
     *
     * <p>Please note: the caller must ensure to take ownership of the returned native pointer,
     * else it will leak!</p>
     */
    @CalledByNative
    public static long getServiceToInject(long nativeClientAndroid) {
        if (sNativeServiceProvider == null) {
            return 0;
        }

        return sNativeServiceProvider.createNativeService(nativeClientAndroid);
    }

    /**
     * Returns the native pointer to the service request sender to inject, or 0 if no service
     * request sender has been set (and the default should be used).
     *
     * <p>Please note: the caller must ensure to take ownership of the returned native pointer,
     * else it will leak!</p>
     */
    @CalledByNative
    public static long getServiceRequestSenderToInject() {
        if (sNativeServiceRequestSenderProvider == null) {
            return 0;
        }

        return sNativeServiceRequestSenderProvider.createNativeServiceRequestSender();
    }

    /**
     * Returns the native pointer to the TTS controller to inject, or 0 if no TTS controller has
     * been set (and the default should be used).
     *
     * <p>Please note: the caller must ensure to take ownership of the returned native pointer,
     * else it will leak!</p>
     */
    @CalledByNative
    public static long getTtsControllerToInject() {
        if (sNativeTtsControllerProvider == null) {
            return 0;
        }

        return sNativeTtsControllerProvider.createNativeTtsController();
    }

    /**
     * Returns whether a provider for a service request sender to inject has been provided.
     * Generally, this means that we are in a test environment.
     */
    public static boolean hasServiceRequestSenderToInject() {
        return sNativeServiceRequestSenderProvider != null;
    }

    /**
     * Returns whether a provider for a TTS controller to inject has been provided.
     */
    public static boolean hasTtsControllerToInject() {
        return sNativeTtsControllerProvider != null;
    }
}
