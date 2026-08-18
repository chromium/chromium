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
import org.chromium.chrome.browser.extensions.api.messaging.IBrowserNativeMessageService;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageCallback;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessagePort;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageService;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

/** Android test Service equivalent of echo.py for browser tests. */
public class EchoService extends Service {
    private static final String TAG = "EchoService";
    public static final String ALLOWED_EXTENSION_ID = "knldjmfmopnpolahpmmgbagdohdnhkik";

    // Multiplexes sessions per extension ID.
    private final Map<String, EchoExtensionService> mSessions =
            Collections.synchronizedMap(new HashMap<>());

    private final IBrowserNativeMessageService.Stub mBinder =
            new IBrowserNativeMessageService.Stub() {
                @Override
                public IExtensionNativeMessageService connectExtension(
                        String extensionId, Bundle extensionInfo) {
                    Log.d(TAG, "connectExtension called for: %s", extensionId);
                    if (!ALLOWED_EXTENSION_ID.equals(extensionId)) {
                        Log.w(TAG, "Rejecting unauthorized extension: %s", extensionId);
                        throw new SecurityException("Unauthorized extension: " + extensionId);
                    }
                    return mSessions.computeIfAbsent(
                            extensionId, id -> new EchoExtensionService(id));
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind called with intent: %s", intent);
        return mBinder;
    }

    private static class EchoExtensionService extends IExtensionNativeMessageService.Stub {
        private final String mExtensionId;
        private final AtomicInteger mPortIdCounter = new AtomicInteger(0);

        EchoExtensionService(String extensionId) {
            mExtensionId = extensionId;
        }

        @Override
        public IExtensionNativeMessagePort connectPort(IExtensionNativeMessageCallback cb) {
            int portId = mPortIdCounter.incrementAndGet();
            Log.d(TAG, "connectPort for extension %s, portId=%d", mExtensionId, portId);
            return new EchoPort(mExtensionId, portId, cb);
        }
    }

    private static class EchoPort extends IExtensionNativeMessagePort.Stub {
        private final String mExtensionId;
        private final int mPortId;
        private final IExtensionNativeMessageCallback mCallback;
        private int mMessageNumber;

        EchoPort(String extensionId, int portId, IExtensionNativeMessageCallback callback) {
            mExtensionId = extensionId;
            mPortId = portId;
            mCallback = callback;
        }

        @Override
        public void postMessage(String messageJson) {
            Log.d(TAG, "Port %d received message: %s", mPortId, messageJson);
            try {
                JSONObject input = new JSONObject(messageJson);

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
