// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.perf;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.benchmark.macro.junit4.BaselineProfileRule;
import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;

import kotlin.Unit;

/**
 * This test creates a baseline profile for chrome. See
 * https://developer.android.com/topic/performance/baselineprofiles/overview
 */
@DoNotBatch(reason = "This test communicates with the runner using Instrumentation Statuses.")
@RunWith(AndroidJUnit4.class)
public class BaselineProfileTest {
    private static final String DATA_URL = "data:,Hello";
    private static final String ACTIVITY_NAME = "com.google.android.apps.chrome.Main";
    private static final String CCT_SESSION_EXTRA = "android.support.customtabs.extra.SESSION";
    private static final String PACKAGE_NAME_ARG = "org.chromium.chrome.test.pagecontroller.rules."
            + "ChromeUiApplicationTestRule.PackageUnderTest";

    @Rule
    public BaselineProfileRule mBaselineProfileRule = new BaselineProfileRule();
    private String mPackageName;

    @Before
    public void setUp() {
        mPackageName = InstrumentationRegistry.getArguments().getString(
                PACKAGE_NAME_ARG, "org.chromium.chrome");
    }

    @Test
    @LargeTest
    public void testGenerateBaselineProfile() {
        Context context = ApplicationProvider.getApplicationContext();
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(DATA_URL));
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        // Mark it as a CCT session.
        intent.putExtra(CCT_SESSION_EXTRA, (Bundle) null);
        intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
        mBaselineProfileRule.collectBaselineProfile(
                /* packageName= */ mPackageName,
                /* iterations= */ 5,
                /* profileBlock= */
                scope -> {
                    scope.startActivityAndWait(intent);
                    return Unit.INSTANCE;
                });
    }
}
