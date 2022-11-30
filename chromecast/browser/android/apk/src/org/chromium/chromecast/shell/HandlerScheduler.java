// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.os.Handler;
import android.os.Looper;

import org.chromium.chromecast.base.Observable.Scheduler;

/**
 * A thin adapter layer that allows using Android Handlers with Observable methods like alarm() and
 * delay().
 *
 * Example:
 *
 *   Observable<Client> clients = ...;
 *   Subscription sub = clients.delay(HandlerScheduler.onCurrentThread(), 200).subscribe(client -> {
 *       // This code is only invoked 200 ms after |clients| is actually activated.
 *       ...
 *   });
 */
public class HandlerScheduler {
    public static Scheduler fromHandler(Handler handler) {
        return handler::postDelayed;
    }

    public static Scheduler onCurrentThread() {
        return fromHandler(new Handler(Looper.myLooper()));
    }
}