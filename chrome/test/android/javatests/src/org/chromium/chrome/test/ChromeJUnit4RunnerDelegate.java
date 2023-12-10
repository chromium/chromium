// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;

import org.chromium.base.test.params.ParameterizedRunner.ParameterizedTestInstantiationException;
import org.chromium.base.test.params.ParameterizedRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunnerDelegateCommon;

import java.util.List;

/** A custom runner delegate for running //chrome JUnit4 parameterized tests. */
public final class ChromeJUnit4RunnerDelegate extends ChromeJUnit4ClassRunner
        implements ParameterizedRunnerDelegate {
    private final ParameterizedRunnerDelegateCommon mDelegateCommon;

    public ChromeJUnit4RunnerDelegate(
            Class<?> klass, ParameterizedRunnerDelegateCommon delegateCommon)
            throws InitializationError {
        super(klass);
        mDelegateCommon = delegateCommon;
    }

    @Override
    public void collectInitializationErrors(List<Throwable> errors) {
        ParameterizedRunnerDelegateCommon.collectInitializationErrors(errors);
    }

    @Override
    public List<FrameworkMethod> computeTestMethods() {
        return mDelegateCommon.computeTestMethods();
    }

    @Override
    public Object createTest() throws ParameterizedTestInstantiationException {
        return mDelegateCommon.createTest();
    }
}
