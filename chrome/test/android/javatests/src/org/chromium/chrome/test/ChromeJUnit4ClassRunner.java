// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.rules.TestRule;
import org.junit.runners.model.InitializationError;

import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.List;

/** A custom runner for //chrome JUnit4 tests. */
public class ChromeJUnit4ClassRunner extends ContentJUnit4ClassRunner {
    /**
     * Create a ChromeJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public ChromeJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    protected List<TestHook> getPreTestHooks() {
        return addToList(super.getPreTestHooks(), Policies.getRegistrationHook());
    }

    @Override
    protected List<TestRule> getDefaultTestRules() {
        List<TestRule> rules = super.getDefaultTestRules();
        return addToList(rules, new DisableAnimationsTestRule());
    }
}
