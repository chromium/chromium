// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.ui;

import android.support.test.rule.ActivityTestRule;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.test.DisableNativeTestRule;

/**
 * Test case to instrument DummyUiActivity for UI testing scenarios.
 * Recommend to use setUpTest() and tearDownTest() to setup and tear down instead of @Before and
 * @After.
 */
public class DummyUiActivityTestCase {
    private DummyUiActivity mActivity;

    private ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public TestRule ruleChain = RuleChain.outerRule(mActivityTestRule)
                                        .around(new DisableNativeTestRule())
                                        .around(new TestDriverRule());

    /**
     * TestRule to setup and tear down for each test.
     */
    public final class TestDriverRule implements TestRule {
        @Override
        public Statement apply(final Statement base, Description description) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    setUpTest();
                    try {
                        base.evaluate();
                    } finally {
                        tearDownTest();
                    }
                }
            };
        }
    }

    // Override this to setup before test.
    public void setUpTest() throws Exception {
        mActivity = mActivityTestRule.getActivity();
    }

    // Override this to tear down after test.
    public void tearDownTest() throws Exception {
    }

    public DummyUiActivity getActivity() {
        return mActivity;
    }

    public ActivityTestRule<DummyUiActivity> getActivityTestRule() {
        return mActivityTestRule;
    }
}
