// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Checks that all requirements set for a BackgroundTask are met and if so, starts running the task.
 *
 * Receives the information through a broadcast, which is synchronous in the Main thread. The
 * execution of the task will be detached to a different thread and the program will wait for the
 * task to finish through a separate Waiter class.
 */
public class BackgroundTaskBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        // TODO(crbug.com/970160): Implement general logic.

        // Not implemented. This assures the method is not called by mistake.
        assert false;
    }
}
