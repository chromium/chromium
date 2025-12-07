// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.junit.rules.ExternalResource;

import org.chromium.build.annotations.NullMarked;

/** A JUnit rule that installs and uninstalls a {@link FakeExtensionActionsBridge} for tests. */
@NullMarked
public class FakeExtensionActionsBridgeRule extends ExternalResource {
    private final FakeExtensionActionsBridge mFakeBridge = new FakeExtensionActionsBridge();

    public FakeExtensionActionsBridgeRule() {}

    /** Returns the {@link FakeExtensionActionsBridge} managed by this rule. */
    public FakeExtensionActionsBridge getFakeBridge() {
        return mFakeBridge;
    }

    @Override
    protected void before() {
        mFakeBridge.clear();
        mFakeBridge.install();
    }

    @Override
    protected void after() {
        mFakeBridge.uninstall();
    }
}
