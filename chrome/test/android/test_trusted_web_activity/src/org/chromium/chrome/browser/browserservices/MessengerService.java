// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import androidx.annotation.Nullable;

/**
 * The TrustedWebActivityClientTest tests need to test behaviour called on
 * TestTrustedWebActivityService. We don't want to add general purpose methods on the
 * TrustedWebActivityService API for testing so we use this class as a side-channel.
 */
public class MessengerService extends Service {
    public static final int MSG_RESPONDER_REGISTERED = 0;
    public static final int MSG_GET_SMALL_ICON_ID = 1;
    public static final int MSG_NOTIFY_NOTIFICATION = 2;
    public static final int MSG_CANCEL_NOTIFICATION = 3;
    public static final String TAG_KEY = "tag";
    public static final String ID_KEY = "id";
    public static final String CHANNEL_KEY = "channel";

    public static MessageHandler sMessageHandler;
    private Messenger mMessenger;

    @Override
    public void onCreate() {
        super.onCreate();

        sMessageHandler = new MessageHandler();
        mMessenger = new Messenger(sMessageHandler);
    }

    static class MessageHandler extends Handler {
        @Nullable private Messenger mResponder;

        @Override
        public void handleMessage(Message msg) {
            assert mResponder == null;
            mResponder = msg.replyTo;
            respond(MSG_RESPONDER_REGISTERED);
        }

        public void recordGetSmallIconId() {
            respond(MSG_GET_SMALL_ICON_ID);
        }

        public void recordNotifyNotification(String tag, int id, String channelName) {
            Bundle bundle = new Bundle();
            bundle.putString(TAG_KEY, tag);
            bundle.putInt(ID_KEY, id);
            bundle.putString(CHANNEL_KEY, channelName);
            respond(MSG_NOTIFY_NOTIFICATION, bundle);
        }

        public void recordCancelNotification(String tag, int id) {
            Bundle bundle = new Bundle();
            bundle.putString(TAG_KEY, tag);
            bundle.putInt(ID_KEY, id);
            respond(MSG_CANCEL_NOTIFICATION, bundle);
        }

        private void respond(int what) {
            respond(what, null);
        }

        private void respond(int what, Bundle data) {
            Message message = Message.obtain();
            message.what = what;
            if (data != null) message.setData(data);

            try {
                mResponder.send(message);
            } catch (RemoteException ex) {
                throw new RuntimeException(ex);
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }
}
