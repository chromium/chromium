// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;

import java.lang.ref.WeakReference;

// A singleton class that holds a WeakReference to the Activity object of GroupedWebsiteSettings.
// Needed to be able to go to the 'All Sites' level when clearing data in SingleWebsiteSettings.
class GroupedWebsitesActivityHolder {
    @Nullable private WeakReference<Activity> mActivity;

    private static GroupedWebsitesActivityHolder sInstance;

    private GroupedWebsitesActivityHolder() {}
    ;

    public static GroupedWebsitesActivityHolder getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new GroupedWebsitesActivityHolder();
        return sInstance;
    }

    public void setActivity(Activity activity) {
        mActivity = new WeakReference<Activity>(activity);
    }

    public Activity getActivity() {
        return mActivity.get();
    }
}
