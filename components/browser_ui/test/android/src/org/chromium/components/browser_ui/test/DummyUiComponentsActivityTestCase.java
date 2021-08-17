// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.test;

import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.ThemedDummyUiActivityTestRule;

/**
 * Wrapper around {@link DummyUiActivityTestCase} with components color
 * overlay.
 */
public class DummyUiComponentsActivityTestCase extends DummyUiActivityTestCase {
    /** Default constructor. */
    public DummyUiComponentsActivityTestCase() {
        super(new ThemedDummyUiActivityTestRule<>(DummyUiActivity.class, R.style.ColorOverlay));
    }
}
