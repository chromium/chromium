// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.echoservices;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.api.messaging.IBrowserNativeMessageService;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageCallback;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessagePort;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** Android test Service equivalent of echo.py for browser tests. */
public class EchoService extends Service {
    private static final String TAG = "EchoService";

    // Always reject incoming connectExtension calls from an extension with this ID.
    public static final String UNAUTHORIZED_EXTENSION_ID = "ddchlicdkolnonkihahngkmmmjnjlkkf";

    // For other extension IDs, always accept incoming connectExtension calls and parrot back the
    // isVerified value in the echo response. This allows tests to verify that Chrome passed the
    // correct isVerified boolean across AIDL without having tests rely on connection rejection,
    // which Chrome surfaces to extensions as a generic "Unable to connect" error indistinguishable
    // from other connection failures.

    // Multiplexes sessions per extension ID.
    private final Map<String, EchoExtensionService> mSessions =
            Collections.synchronizedMap(new HashMap<>());

    // Tracks extension IDs for which closeConnection() was called.
    private final Set<String> mUnloadedExtensions = Collections.synchronizedSet(new HashSet<>());

    // Callbacks waiting for a particular extension to unload.
    private final Map<String, List<IExtensionNativeMessageCallback>> mUnloadWaiters =
            Collections.synchronizedMap(new HashMap<>());

    // Callbacks waiting for a particular extension to connect.
    private final Map<String, List<IExtensionNativeMessageCallback>> mExtensionConnectedWaiters =
            Collections.synchronizedMap(new HashMap<>());

    private static String createStatusReply(String status, String extensionId)
            throws JSONException {
        JSONObject reply = new JSONObject();
        reply.put("status", status);
        reply.put("extensionId", extensionId);
        return reply.toString();
    }

    private void onExtensionUnloaded(String extensionId) {
        mSessions.remove(extensionId);
        mUnloadedExtensions.add(extensionId);

        // Notify waiting extensions that an extension with `extensionId` was unloaded.
        List<IExtensionNativeMessageCallback> waiters = mUnloadWaiters.remove(extensionId);
        if (waiters != null) {
            for (IExtensionNativeMessageCallback waiter : waiters) {
                try {
                    waiter.onMessage(createStatusReply("unloaded", extensionId));
                } catch (JSONException | RemoteException e) {
                    Log.e(TAG, "Failed to notify unload waiter for " + extensionId, e);
                }
            }
        }
    }

    private final IBrowserNativeMessageService.Stub mBinder =
            new IBrowserNativeMessageService.Stub() {
                @Override
                public IExtensionNativeMessageService connectExtension(
                        String extensionId, Bundle extensionInfo) {
                    Log.d(TAG, "connectExtension called for: %s", extensionId);
                    if (UNAUTHORIZED_EXTENSION_ID.equals(extensionId)) {
                        Log.w(TAG, "Rejecting unauthorized extension: %s", extensionId);
                        throw new SecurityException("Unauthorized extension: " + extensionId);
                    }

                    // Record isVerified to parrot back in echo reply.
                    boolean isVerified = extensionInfo.getBoolean("isVerified", false);
                    EchoExtensionService service =
                            mSessions.computeIfAbsent(
                                    extensionId, id -> new EchoExtensionService(id, isVerified));

                    // Notify waiting extensions that an extension with `extensionId` was connected.
                    List<IExtensionNativeMessageCallback> extensionConnectedWaiters =
                            mExtensionConnectedWaiters.remove(extensionId);
                    if (extensionConnectedWaiters != null) {
                        for (IExtensionNativeMessageCallback waiter : extensionConnectedWaiters) {
                            try {
                                waiter.onMessage(createStatusReply("loaded", extensionId));
                            } catch (JSONException | RemoteException e) {
                                Log.e(TAG, "Failed to notify load waiter for " + extensionId, e);
                            }
                        }
                    }
                    return service;
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind called with intent: %s", intent);
        return mBinder;
    }

    private class EchoExtensionService extends IExtensionNativeMessageService.Stub {
        private final String mExtensionId;
        private final @Nullable Boolean mIsVerified;
        private final AtomicInteger mPortIdCounter = new AtomicInteger(0);

        EchoExtensionService(String extensionId, @Nullable Boolean isVerified) {
            mExtensionId = extensionId;
            mIsVerified = isVerified;
        }

        @Override
        public void closeConnection() {
            Log.d(TAG, "closeConnection called for: %s", mExtensionId);
            onExtensionUnloaded(mExtensionId);
        }

        @Override
        public IExtensionNativeMessagePort connectPort(IExtensionNativeMessageCallback cb) {
            int portId = mPortIdCounter.incrementAndGet();
            Log.d(TAG, "connectPort for extension %s, portId=%d", mExtensionId, portId);
            return new EchoPort(mExtensionId, mIsVerified, portId, cb);
        }
    }

    private class EchoPort extends IExtensionNativeMessagePort.Stub {
        private final String mExtensionId;
        private final @Nullable Boolean mIsVerified;
        private final int mPortId;
        private final IExtensionNativeMessageCallback mCallback;
        private int mMessageNumber;

        EchoPort(
                String extensionId,
                @Nullable Boolean isVerified,
                int portId,
                IExtensionNativeMessageCallback callback) {
            mExtensionId = extensionId;
            mIsVerified = isVerified;
            mPortId = portId;
            mCallback = callback;
        }

        @Override
        public void postMessage(String messageJson) {
            Log.d(TAG, "Port %d received message: %s", mPortId, messageJson);
            try {
                JSONObject input = new JSONObject(messageJson);

                // Wait for extension connected request.
                // - if the extension is already connected, reply immediately with the connected
                // extension's ID.
                // - otherwise, puts the `mCallback` in a list to be notified later when the
                // specified extension connects.
                if ("waitForExtensionConnected".equalsIgnoreCase(input.optString("request"))) {
                    String targetId = input.getString("extensionId");
                    if (mSessions.containsKey(targetId)) {
                        mCallback.onMessage(createStatusReply("loaded", targetId));
                    } else {
                        mExtensionConnectedWaiters
                                .computeIfAbsent(
                                        targetId,
                                        k -> Collections.synchronizedList(new ArrayList<>()))
                                .add(mCallback);
                    }
                    return;
                }

                // Wait for extension unloaded request.
                // - if the extension is already unloaded, reply immediately with the unloaded
                // extension's ID.
                // - otherwise, puts the `mCallback` in a list to be notified later when the
                // specified extension unloads.
                if ("waitForExtensionUnloaded".equalsIgnoreCase(input.optString("request"))) {
                    String targetId = input.getString("extensionId");
                    if (mUnloadedExtensions.contains(targetId)) {
                        mCallback.onMessage(createStatusReply("unloaded", targetId));
                    } else {
                        mUnloadWaiters
                                .computeIfAbsent(
                                        targetId,
                                        k -> Collections.synchronizedList(new ArrayList<>()))
                                .add(mCallback);
                    }
                    return;
                }

                // Edge Case 1: stopHostTest -> simulates host disconnecting/exiting
                if (input.optBoolean("stopHostTest", false)) {
                    mCallback.onDisconnect();
                    return;
                }

                // Edge Case 2: sendInvalidResponse -> malformed JSON
                if (input.optBoolean("sendInvalidResponse", false)) {
                    mCallback.onMessage("{");
                    return;
                }

                // TODO(crbug.com/515159909): handle messages that would exceed the size threshold
                // of TransactionTooLargeException.
                // The edge case optBoolean is "bigMessageTest"

                mMessageNumber++;
                JSONObject reply = new JSONObject();
                reply.put("id", mMessageNumber);
                reply.put("echo", input);
                reply.put("caller_url", "chrome-extension://" + mExtensionId + "/");
                if (mIsVerified != null) {
                    reply.put("isVerified", mIsVerified);
                }

                mCallback.onMessage(reply.toString());
            } catch (JSONException e) {
                Log.e(TAG, "Failed to parse incoming message as JSON", e);
            } catch (RemoteException e) {
                Log.e(TAG, "RemoteException sending reply on port %d", mPortId, e);
            }
        }

        @Override
        public void disconnect() {
            Log.d(TAG, "disconnect called for port %d", mPortId);
            try {
                mCallback.onDisconnect();
            } catch (RemoteException ignored) {
            }
        }
    }
}
