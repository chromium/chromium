// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.GcmTaskService;
import com.google.android.gms.gcm.Task;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.util.HashSet;
import java.util.Set;

/**
 * Custom shadow for the OS's GcmNetworkManager.  We use this to hook the call to GcmNetworkManager
 * to make sure it was invoked as we expect.
 */
@Implements(GcmNetworkManager.class)
public class ShadowGcmNetworkManager {
    private Task mTask;
    private Task mCanceledTask;
    private Set<String> mCanceledTaskTags;

    public ShadowGcmNetworkManager() {
        mCanceledTaskTags = new HashSet<>();
    }

    @Implementation
    public void schedule(Task task) {
        mTask = task;
        mCanceledTask = null;
    }

    @Implementation
    public void cancelTask(String tag, Class<? extends GcmTaskService> gcmTaskService) {
        if (mTask != null && mTask.getTag().equals(tag)
                && mTask.getServiceName().equals(gcmTaskService.getName())) {
            mCanceledTask = mTask;
            mTask = null;
        }
        mCanceledTaskTags.add(tag);
    }

    public Task getScheduledTask() {
        return mTask;
    }

    public Task getCanceledTask() {
        return mCanceledTask;
    }

    public Set<String> getCanceledTaskTags() {
        return mCanceledTaskTags;
    }

    public void clear() {
        mTask = null;
        mCanceledTask = null;
        mCanceledTaskTags = new HashSet<>();
    }
}
