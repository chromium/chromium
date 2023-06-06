// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.OnlyRunJavaCronet;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.JavaCronetEngine;

/**
 * Tests features of CronetTestRule.
 */
@RunWith(AndroidJUnit4.class)
public class CronetTestRuleTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();
    @Rule
    public final TestName mTestName = new TestName();
    /**
     * For any test whose name contains "MustRun", it's enforced that the test must run and set
     * {@code mTestWasRun} to {@code true}.
     */
    private boolean mTestWasRun;

    @Before
    public void setUp() throws Exception {
        mTestWasRun = false;
    }

    @After
    public void tearDown() throws Exception {
        if (mTestName.getMethodName().contains("MustRun") && !mTestWasRun) {
            fail(mTestName.getMethodName() + " should have run but didn't.");
        }
    }

    @Test
    @SmallTest
    @RequiresMinApi(999999999)
    public void testRequiresMinApiDisable() {
        fail("RequiresMinApi failed to disable.");
    }

    @Test
    @SmallTest
    @RequiresMinApi(-999999999)
    public void testRequiresMinApiMustRun() {
        mTestWasRun = true;
    }

    @Test
    @SmallTest
    public void testRunBothImplsMustRun() {
        if (mTestRule.testingJavaImpl()) {
            assertThat(mTestWasRun).isFalse();
            mTestWasRun = true;
            assertThat(mTestRule.getTestFramework().getEngine())
                    .isInstanceOf(JavaCronetEngine.class);
        } else {
            assertThat(mTestWasRun).isFalse();
            mTestWasRun = true;
            assertThat(mTestRule.getTestFramework().getEngine())
                    .isInstanceOf(CronetUrlRequestContext.class);
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testRunOnlyNativeMustRun() {
        assertThat(mTestRule.testingJavaImpl()).isFalse();
        assertThat(mTestWasRun).isFalse();
        mTestWasRun = true;
        assertThat(mTestRule.getTestFramework().getEngine())
                .isInstanceOf(CronetUrlRequestContext.class);
    }

    @Test
    @SmallTest
    @OnlyRunJavaCronet
    public void testRunOnlyJavaMustRun() {
        assertThat(mTestRule.testingJavaImpl()).isTrue();
        assertThat(mTestWasRun).isFalse();
        mTestWasRun = true;
        assertThat(mTestRule.getTestFramework().getEngine()).isInstanceOf(JavaCronetEngine.class);
    }
}
