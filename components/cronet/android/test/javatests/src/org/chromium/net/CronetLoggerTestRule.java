// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.NonNull;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.net.impl.CronetLogger;
import org.chromium.net.impl.CronetLoggerFactory.SwapLoggerForTesting;

import java.util.Objects;

/**
 * Custom TestRule that instantiates a new fake CronetLogger for each test.
 *
 * @param <T> The actual type of the class extending CronetLogger.
 */
public class CronetLoggerTestRule<T extends CronetLogger> implements TestRule {
    private Class<T> mTestLoggerClazz;

    // Expose the fake logger to the test.
    public T mTestLogger;

    public CronetLoggerTestRule(@NonNull Class<T> testLoggerClazz) {
        mTestLoggerClazz = Objects.requireNonNull(testLoggerClazz, "TestLoggerClazz is required.");
    }

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try (SwapLoggerForTesting swapper = buildSwapper()) {
                    base.evaluate();
                } finally {
                    mTestLogger = null;
                }
            }
        };
    }

    private SwapLoggerForTesting buildSwapper() {
        assert mTestLoggerClazz != null;

        try {
            mTestLogger = mTestLoggerClazz.getConstructor().newInstance();
            return new SwapLoggerForTesting(mTestLogger);
        } catch (ReflectiveOperationException e) {
            throw new IllegalArgumentException(
                    "CronetTestBase#runTest failed while swapping TestLogger.", e);
        }
    }
}
