// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.junit.rules.ExternalResource;

import org.chromium.build.annotations.NullMarked;

/**
 * A JUnit rule that activates a {@link FakeExtensionUiBackend} for tests.
 *
 * <p>{@link ExtensionUiBackendImpl}, the default implementation of {@link ExtensionUiBackend},
 * calls into {@link ExtensionActionsBridge}, so you may want to consider {@link
 * FakeExtensionActionsBridgeRule} instead if your tests run on extension-enabled builds only. This
 * fake is useful on testing a class that uses {@link ExtensionUi} and is compiled on all platforms
 * because {@link ExtensionActionsBridge} is not compiled if extensions are disabled.
 */
@NullMarked
public class FakeExtensionUiBackendRule extends ExternalResource {
    private final FakeExtensionUiBackend mFakeBackend;

    public FakeExtensionUiBackendRule() {
        mFakeBackend = new FakeExtensionUiBackend();
    }

    /**
     * Sets whether the extension UI is enabled or not. It applies to all profiles.
     *
     * <p>By default, it returns true if extensions are enabled on this build; otherwise false.
     */
    public void setEnabled(boolean enabled) {
        mFakeBackend.setEnabled(enabled);
    }

    @Override
    protected void before() {
        ExtensionUi.setBackendForTesting(mFakeBackend);
    }

    @Override
    protected void after() {
        ExtensionUi.setBackendForTesting(null);
    }
}
