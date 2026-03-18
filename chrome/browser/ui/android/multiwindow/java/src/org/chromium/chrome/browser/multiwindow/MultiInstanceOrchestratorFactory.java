// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Factory for {@link MultiInstanceOrchestrator}. This class is in a low-level package to allow
 * modularized components to access the singleton without depending on the full app implementation.
 *
 * <p>The implementation should be registered by the app layer at startup using {@link
 * #setInstance(MultiInstanceOrchestrator)}.
 */
@NullMarked
public class MultiInstanceOrchestratorFactory {
    private static @Nullable MultiInstanceOrchestrator sInstance;

    protected MultiInstanceOrchestratorFactory() {}

    /** Returns the {@link MultiInstanceOrchestrator} instance. */
    public static MultiInstanceOrchestrator getInstance() {
        if (sInstance != null) return sInstance;
        throw new IllegalStateException("MultiInstanceOrchestrator has not been registered.");
    }

    /**
     * Registers the {@link MultiInstanceOrchestrator} instance.
     *
     * @param orchestrator The instance to register.
     */
    protected static void setInstance(MultiInstanceOrchestrator orchestrator) {
        sInstance = orchestrator;
    }

    public static void setInstanceForTesting(MultiInstanceOrchestrator orchestrator) {
        sInstance = orchestrator;
        ResettersForTesting.register(() -> sInstance = null);
    }
}
