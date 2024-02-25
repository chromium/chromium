// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.compositor.layouts;

import org.junit.rules.ExternalResource;

import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;

/** JUnit 4 rule that disables animations in CompositorAnimationHandler for tests. */
public class DisableChromeAnimations extends ExternalResource {
    private boolean mOldTestingMode;

    @Override
    protected void before() {
        mOldTestingMode = CompositorAnimationHandler.isInTestingMode();
        CompositorAnimationHandler.setTestingMode(true);
    }

    @Override
    protected void after() {
        CompositorAnimationHandler.setTestingMode(mOldTestingMode);
    }
}
