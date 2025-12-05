// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
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
    /** Command to the service to handle a FedCM request */
    private static final int MSG_FEDCM_REQUEST = 1;

    private static final int MSG_FEDCM_RESPONSE = 2;

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

            if (msg.what == MSG_FEDCM_REQUEST) {
                Messenger replyTo = msg.replyTo;
                String request = msg.getData().getString("request");

                mExecutor.execute(
                        () -> {
                            Message replyMsg = Message.obtain();
                            replyMsg.what = MSG_FEDCM_RESPONSE;
                            Bundle bundle = new Bundle();
                            bundle.putString("reply", request + "Hello world!");
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
