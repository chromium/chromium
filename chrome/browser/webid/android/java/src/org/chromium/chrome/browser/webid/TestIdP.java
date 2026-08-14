// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import org.chromium.base.Log;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * This is a test Identity Provider (IdP) that responds to FedCM requests via bound services.
 *
 * <p>It is only used in Android browser tests and is not exposed in clank.
 */
public class TestIdP extends Service {
    // Use constants from IdentityProviderService.

    private Messenger mMessenger;
    private ExecutorService mExecutor;

    @Override
    public void onCreate() {
        super.onCreate();
        mExecutor = Executors.newSingleThreadExecutor();
    }

    static class IncomingHandler extends Handler {
        private static final String TAG = "TestIdP";

        private final Context mApplicationContext;
        private final ExecutorService mExecutor;

        IncomingHandler(Context context, ExecutorService executor) {
            super(Looper.getMainLooper());
            this.mApplicationContext = context;
            this.mExecutor = executor;
        }

        @Override
        public void handleMessage(Message msg) {
            Log.v(TAG, "Got a message! Yay?");

            String callingApp =
                    mApplicationContext.getPackageManager().getNameForUid(msg.sendingUid);
            if (!"org.chromium.android_browsertests_apk".equals(callingApp)) {
                Log.w(TAG, "Unauthorized client: " + callingApp);
                return;
            }

            if (msg.what == IdentityProviderService.MSG_FEDCM_REQUEST) {
                Messenger replyTo = msg.replyTo;
                Bundle data = msg.getData();
                String url = data.getString(IdentityProviderService.FEDCM_BOUND_SERVICE_INTENT_URL);
                String body =
                        data.getString(IdentityProviderService.FEDCM_BOUND_SERVICE_INTENT_BODY);
                StringBuilder extraInfo = new StringBuilder();
                if (body != null && !body.isEmpty()) {
                    extraInfo.append(":").append(body);
                }
                Bundle headersBundle =
                        data.getBundle(IdentityProviderService.FEDCM_BOUND_SERVICE_INTENT_HEADERS);
                if (headersBundle != null) {
                    java.util.List<String> headerKeys =
                            new java.util.ArrayList<>(headersBundle.keySet());
                    java.util.Collections.sort(headerKeys);
                    for (String key : headerKeys) {
                        extraInfo
                                .append(":header:")
                                .append(key)
                                .append("=")
                                .append(headersBundle.getString(key));
                    }
                }
                final String replyString;
                String path = url != null ? Uri.parse(url).getPath() : null;
                if (path != null && path.endsWith("/no_reply")) {
                    // Do not reply, keeping the request in flight.
                    return;
                }
                if (path != null && path.endsWith("/json_accounts")) {
                    replyString =
                            "{\"accounts\": [{\"id\": \"1234\", \"name\": \"Jane Doe\","
                                    + " \"email\": \"jane@idp.example\", \"given_name\":"
                                    + " \"Jane\"}]}";
                } else if (path != null && path.endsWith("/json_token")) {
                    replyString = "{\"token\": \"sample_native_jwt_token_12345\"}";
                } else if (path != null && path.endsWith("/json_continue")) {
                    replyString = "{\"continue_on\": \"https://idp.example/fedcm/continue\"}";
                } else if (path != null && path.endsWith("/json_error")) {
                    replyString = "{\"error\": {\"code\": \"access_denied\"}}";
                } else {
                    replyString = (url != null ? url : "") + extraInfo.toString() + "Hello world!";
                }

                mExecutor.execute(
                        () -> {
                            Message replyMsg = Message.obtain();
                            replyMsg.what = IdentityProviderService.MSG_FEDCM_RESPONSE;
                            Bundle bundle = new Bundle();
                            bundle.putString(
                                    IdentityProviderService.FEDCM_BOUND_SERVICE_INTENT_REPLY,
                                    replyString);
                            replyMsg.setData(bundle);
                            try {
                                Log.v(TAG, "Replying!");
                                replyTo.send(replyMsg);
                            } catch (RemoteException e) {
                                Log.v(TAG, "Oops");
                                Log.v(TAG, e.toString());
                                e.printStackTrace();
                            }
                        });
            } else {
                Log.v(TAG, "Ooops, not really!");
                super.handleMessage(msg);
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        mMessenger = new Messenger(new IncomingHandler(getApplicationContext(), mExecutor));
        return mMessenger.getBinder();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mExecutor != null) {
            mExecutor.shutdown();
        }
    }
}
