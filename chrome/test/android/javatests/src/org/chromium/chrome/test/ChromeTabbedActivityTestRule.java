// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.ActivityOptions;
import android.content.Intent;
import android.os.Bundle;

import org.junit.Assert;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.util.ApplicationTestUtils;

/**
 * Custom ActivityTestRule for tests using ChromeTabbedActivity
 */
public class ChromeTabbedActivityTestRule extends ChromeActivityTestRule<ChromeTabbedActivity> {
    public ChromeTabbedActivityTestRule() {
        super(ChromeTabbedActivity.class);
    }

    private Bundle noAnimationLaunchOptions() {
        return ActivityOptions.makeCustomAnimation(getActivity(), 0, 0).toBundle();
    }

    public void resumeMainActivityFromLauncher() throws Exception {
        Assert.assertNotNull(getActivity());
        Assert.assertEquals(
                ApplicationStatus.getStateForActivity(getActivity()), ActivityState.STOPPED);

        Intent launchIntent = getActivity().getPackageManager().getLaunchIntentForPackage(
                getActivity().getPackageName());
        getActivity().startActivity(launchIntent, noAnimationLaunchOptions());
        ApplicationTestUtils.waitForActivityState(getActivity(), ActivityState.RESUMED);
    }
}
