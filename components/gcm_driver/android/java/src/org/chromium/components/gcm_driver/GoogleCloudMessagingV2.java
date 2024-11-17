// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageUtils;

import java.io.IOException;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Temporary code for sending subtypes when (un)subscribing with GCM.
 * Subtypes are experimental and may change without notice!
 * TODO(johnme): Remove this file, once we switch to the GMS client library.
 */
public class GoogleCloudMessagingV2 implements GoogleCloudMessagingSubscriber {
    private static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";
    private static final long REGISTER_TIMEOUT = 5000;
    private static final String ACTION_C2DM_REGISTER = "com.google.android.c2dm.intent.REGISTER";
    private static final String C2DM_EXTRA_ERROR = "error";
    private static final String INTENT_PARAM_APP = "app";
    private static final String ERROR_MAIN_THREAD = "MAIN_THREAD";
    private static final String ERROR_SERVICE_NOT_AVAILABLE = "SERVICE_NOT_AVAILABLE";
    private static final String EXTRA_DELETE = "delete";
    private static final String EXTRA_REGISTRATION_ID = "registration_id";
    private static final String EXTRA_SENDER = "sender";
    private static final String EXTRA_MESSENGER = "google.messenger";
    private static final String EXTRA_SUBTYPE = "subtype";
    private static final String EXTRA_SUBSCRIPTION = "subscription";

    private PendingIntent mAppPendingIntent;
    private final Object mAppPendingIntentLock = new Object();

    public GoogleCloudMessagingV2() {}

    @Override
    public String subscribe(String source, String subtype, @Nullable Bundle data)
            throws IOException {
        if (data == null) {
            data = new Bundle();
        }
        data.putString(EXTRA_SUBTYPE, subtype);
        Bundle result = subscribe(source, data);
        return result.getString(EXTRA_REGISTRATION_ID);
    }

    @Override
    public void unsubscribe(String source, String subtype, @Nullable Bundle data)
            throws IOException {
        if (data == null) {
            data = new Bundle();
        }
        data.putString(EXTRA_SUBTYPE, subtype);
        unsubscribe(source, data);
    }

    /**
     * Subscribe to receive GCM messages from a specific source.
     * <p>
     * Source Types:
     * <ul>
     * <li>Sender ID - if you have multiple senders you can call this method
     * for each additional sender. Each sender can use the corresponding
     * {@link #REGISTRATION_ID} returned in the bundle to send messages
     * from the server.</li>
     * <li>Cloud Pub/Sub topic - You can subscribe to a topic and receive
     * notifications from the owner of that topic, when something changes.
     * For more information see
     * <a href="https://cloud.google.com/pubsub">Cloud Pub/Sub</a>.</li>
     * </ul>
     * This function is blocking and should not be called on the main thread.
     *
     * @param source of the desired notifications.
     * @param data (optional) additional information.
     * @return Bundle containing subscription information including {@link #REGISTRATION_ID}
     * @throws IOException if the request fails.
     */
    public Bundle subscribe(String source, Bundle data) throws IOException {
        if (data == null) {
            data = new Bundle();
        }
        // Expected by older versions of GMS and servlet
        data.putString(EXTRA_SENDER, source);
        // New name of the sender parameter
        data.putString(EXTRA_SUBSCRIPTION, source);
        // DB buster for older versions of GCM.
        if (data.getString(EXTRA_SUBTYPE) == null) {
            data.putString(EXTRA_SUBTYPE, source);
        }

        Intent resultIntent = registerRpc(data);
        getExtraOrThrow(resultIntent, EXTRA_REGISTRATION_ID);
        return resultIntent.getExtras();
    }

    /**
     * Unsubscribe from a source to stop receiving messages from it.
     * <p>
     * This function is blocking and should not be called on the main thread.
     *
     * @param source to unsubscribe
     * @param data (optional) additional information.
     * @throws IOException if the request fails.
     */
    public void unsubscribe(String source, Bundle data) throws IOException {
        if (data == null) {
            data = new Bundle();
        }
        // Use the register servlet, with 'delete=true'.
        // Registration service returns a registration_id on success - or an error code.
        data.putString(EXTRA_DELETE, "1");
        subscribe(source, data);
    }

    private Intent registerRpc(Bundle data) throws IOException {
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(ERROR_MAIN_THREAD);
        }
        if (!PackageUtils.isPackageInstalled(GOOGLE_PLAY_SERVICES_PACKAGE)) {
            throw new IOException("Google Play Services missing");
        }
        if (data == null) {
            data = new Bundle();
        }

        final BlockingQueue<Intent> responseResult = new LinkedBlockingQueue<Intent>();
        Handler responseHandler =
                new Handler(Looper.getMainLooper()) {
                    @Override
                    public void handleMessage(Message msg) {
                        Intent res = (Intent) msg.obj;
                        responseResult.add(res);
                    }
                };
        Messenger responseMessenger = new Messenger(responseHandler);

        Intent intent = new Intent(ACTION_C2DM_REGISTER);
        intent.setPackage(GOOGLE_PLAY_SERVICES_PACKAGE);
        setPackageNameExtra(intent);
        intent.putExtras(data);
        intent.putExtra(EXTRA_MESSENGER, responseMessenger);
        ContextUtils.getApplicationContext().startService(intent);
        try {
            return responseResult.poll(REGISTER_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new IOException(e.getMessage());
        }
    }

    private String getExtraOrThrow(Intent intent, String extraKey) throws IOException {
        if (intent == null) {
            throw new IOException(ERROR_SERVICE_NOT_AVAILABLE);
        }

        String extraValue = intent.getStringExtra(extraKey);
        if (extraValue != null) {
            return extraValue;
        }

        String err = intent.getStringExtra(C2DM_EXTRA_ERROR);
        if (err != null) {
            throw new IOException(err);
        } else {
            throw new IOException(ERROR_SERVICE_NOT_AVAILABLE);
        }
    }

    private void setPackageNameExtra(Intent intent) {
        synchronized (mAppPendingIntentLock) {
            if (mAppPendingIntent == null) {
                Intent target = new Intent();
                // Fill in the package, to prevent the intent from being used.
                target.setPackage("com.google.example.invalidpackage");
                mAppPendingIntent =
                        PendingIntent.getBroadcast(
                                ContextUtils.getApplicationContext(),
                                0,
                                target,
                                IntentUtils.getPendingIntentMutabilityFlag(false));
            }
        }
        intent.putExtra(INTENT_PARAM_APP, mAppPendingIntent);
    }
}
