// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Collections;

/** junit tests for the {@link PickerBitmapView} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PickerBitmapViewTest {
    @Test
    public void testPrematureOnSelectionStateChanged() {
        PickerBitmapView view =
                new PickerBitmapView(RuntimeEnvironment.application.getApplicationContext(), null);
        // Simulate crash scenario in crbug.com/1006823, where an event occurred before
        // PickerBitmapView has been initialized.
        view.onSelectionStateChange(Collections.emptyList());
    }
}
