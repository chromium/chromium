// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.content.Intent;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.content_public.browser.WebContents;

/** Rule for integration tests that start a new {@link CustomTabActivity}. */
@NullMarked
public class CctTransitTestRule implements TestRule {
    private final CustomTabActivityTestRule mActivityTestRule;

    public CctTransitTestRule() {
        mActivityTestRule = new CustomTabActivityTestRule();
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return mActivityTestRule.apply(statement, description);
    }

    public CustomTabActivityTestRule getActivityTestRule() {
        return mActivityTestRule;
    }

    /**
     * Start the test in a CCT page.
     *
     * @return the active entry {@link CctPageStation}
     */
    public CctPageStation startCustomTabActivityWithIntent(Intent intent) {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        CctPageStation page = CctPageStation.newBuilder().withEntryPoint().build();
        return sentinel.runTo(() -> mActivityTestRule.startActivityCompletely(intent))
                .arriveAt(page);
    }

    public CustomTabActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    public AppMenuCoordinator getAppMenuCoordinator() {
        return mActivityTestRule.getAppMenuCoordinator();
    }

    public void setActivity(CustomTabActivity cctActivity) {
        mActivityTestRule.setActivity(cctActivity);
    }

    public void waitForActivityCompletelyLoaded() {
        mActivityTestRule.waitForActivityCompletelyLoaded();
    }

    public WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    public CustomTabActivity launchActivity(Intent intent) {
        return mActivityTestRule.launchActivity(intent);
    }

    public void startActivityCompletely(Intent intent) {
        mActivityTestRule.startActivityCompletely(intent);
    }

    public int tabsCount(boolean incognito) {
        return mActivityTestRule.tabsCount(incognito);
    }
}
