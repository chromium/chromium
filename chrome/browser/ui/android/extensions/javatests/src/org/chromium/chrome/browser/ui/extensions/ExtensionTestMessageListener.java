// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/**
 * Java wrapper for extensions::ExtensionTestMessageListener.
 *
 * <p>This class helps to wait for incoming messages sent from JavaScript via chrome.test APIs.
 *
 * <p>Usage:
 *
 * <pre>
 * try (ExtensionTestMessageListener listener = new ExtensionTestMessageListener("expected")) {
 *     // Trigger action that sends message
 *     assertTrue(listener.waitUntilSatisfied());
 * }
 * </pre>
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionTestMessageListener implements AutoCloseable {
    private long mNativeExtensionTestMessageListenerAndroid;

    /**
     * @param expectedMessage The message to listen for. If null or empty, listens for any message.
     * @param willReply Whether the listener will reply to the message.
     */
    public ExtensionTestMessageListener(@Nullable String expectedMessage, boolean willReply) {
        if (!CommandLine.getInstance().hasSwitch(ChromeSwitches.TEST_TYPE)) {
            throw new IllegalStateException(
                    "ExtensionTestMessageListener must be used in tests with the --test-type"
                            + " flag.");
        }
        mNativeExtensionTestMessageListenerAndroid =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ExtensionTestMessageListenerJni.get()
                                        .create(
                                                expectedMessage == null ? "" : expectedMessage,
                                                willReply));
    }

    /**
     * Convenience constructor that waits for a specific message and does not reply.
     *
     * @param expectedMessage The message to listen for.
     */
    public ExtensionTestMessageListener(String expectedMessage) {
        this(expectedMessage, false);
    }

    /** Convenience constructor that listens for any message and does not reply. */
    public ExtensionTestMessageListener() {
        this("", false);
    }

    @Override
    public void close() {
        destroy();
    }

    public void destroy() {
        assert mNativeExtensionTestMessageListenerAndroid != 0;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ExtensionTestMessageListenerJni.get()
                                .destroy(mNativeExtensionTestMessageListenerAndroid));
        mNativeExtensionTestMessageListenerAndroid = 0;
    }

    /**
     * Waits until the listener receives the expected message.
     *
     * @return true if the message was received, false if interrupted or failed.
     */
    public boolean waitUntilSatisfied() {
        assert mNativeExtensionTestMessageListenerAndroid != 0;
        try {
            CriteriaHelper.pollUiThread(
                    () ->
                            ExtensionTestMessageListenerJni.get()
                                    .wasSatisfied(mNativeExtensionTestMessageListenerAndroid),
                    10000L,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    /**
     * Replies to the last received message.
     *
     * @param message The reply message.
     */
    public void reply(String message) {
        assert mNativeExtensionTestMessageListenerAndroid != 0;
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ExtensionTestMessageListenerJni.get()
                                .reply(mNativeExtensionTestMessageListenerAndroid, message));
    }

    /**
     * @return The last received message.
     */
    public String getMessage() {
        assert mNativeExtensionTestMessageListenerAndroid != 0;
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ExtensionTestMessageListenerJni.get()
                                .getMessage(mNativeExtensionTestMessageListenerAndroid));
    }

    @NativeMethods
    interface Natives {
        long create(@JniType("std::string") String expectedMessage, boolean willReply);

        void destroy(long nativeExtensionTestMessageListenerAndroid);

        boolean wasSatisfied(long nativeExtensionTestMessageListenerAndroid);

        void reply(
                long nativeExtensionTestMessageListenerAndroid,
                @JniType("std::string") String message);

        @JniType("std::string")
        String getMessage(long nativeExtensionTestMessageListenerAndroid);
    }
}
