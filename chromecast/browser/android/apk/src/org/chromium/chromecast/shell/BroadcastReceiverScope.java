// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.chromium.base.ContextUtils;
import org.chromium.chromecast.base.Scope;

/**
 * Registers a BroadcastReceiver in the constructor, and unregisters it in the close() method.
 *
 * This can be used to react to Observables to properly control the lifetimes of BroadcastReceivers.
 */
public class BroadcastReceiverScope implements Scope {
    private final Context mContext;
    private final BroadcastReceiver mReceiver;

    public BroadcastReceiverScope(IntentFilter filter, IntentReceivedCallback callback) {
        this(ContextUtils.getApplicationContext(), filter, callback);
    }

    public BroadcastReceiverScope(
            Context context, IntentFilter filter, IntentReceivedCallback callback) {
        mContext = context;
        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                callback.onReceive(intent);
            }
        };
        mContext.registerReceiver(mReceiver, filter);
    }

    @Override
    public void close() {
        mContext.unregisterReceiver(mReceiver);
    }

    /**
     * Functional interface to handle received Intents.
     */
    public interface IntentReceivedCallback {
        public void onReceive(Intent intent);
    }
}
