// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.chromium.base.ContextUtils;
import org.chromium.chromecast.base.Scope;

/**
 * Registers a BroadcastReceiver in the constructor, and unregisters it in the close() method.
 *
 * This can be used to react to Observables to properly control the lifetimes of BroadcastReceivers.
 */
public class LocalBroadcastReceiverScope implements Scope {
    private final LocalBroadcastManager mBroadcastManager;
    private final BroadcastReceiver mReceiver;

    public LocalBroadcastReceiverScope(IntentFilter filter, IntentReceivedCallback callback) {
        this(LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext()), filter,
                callback);
    }

    public LocalBroadcastReceiverScope(LocalBroadcastManager broadcastManager, IntentFilter filter,
            IntentReceivedCallback callback) {
        mBroadcastManager = broadcastManager;
        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                callback.onReceive(intent);
            }
        };
        mBroadcastManager.registerReceiver(mReceiver, filter);
    }

    @Override
    public void close() {
        mBroadcastManager.unregisterReceiver(mReceiver);
    }

    /**
     * Functional interface to handle received Intents.
     */
    public interface IntentReceivedCallback { public void onReceive(Intent intent); }
}
