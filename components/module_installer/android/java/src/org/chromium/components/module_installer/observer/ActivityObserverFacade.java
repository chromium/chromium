// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observer;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;

import java.util.List;

/**
 * ActivityObserver Context. Class used to segregate external dependencies that
 * cannot be easily mocked and simplify the observer's design.
 */
class ActivityObserverFacade {
    public List<Activity> getRunningActivities() {
        return ApplicationStatus.getRunningActivities();
    }

    public int getStateForActivity(Activity activity) {
        return ApplicationStatus.getStateForActivity(activity);
    }
}
