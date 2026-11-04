// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiEntryTest {

    @Test
    public void testBuilderSuccess() {
        AdaptiveUiEntry entry =
                new AdaptiveUiEntry.Builder()
                        .setScope(AdaptiveUiScope.WINDOW)
                        .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                        .addAllowedContainer(AdaptiveUiContainer.MODAL_DIALOG)
                        .build();

        assertEquals(AdaptiveUiScope.WINDOW, entry.getScope());
        int expectedMask = AdaptiveUiContainer.BOTTOM_SHEET | AdaptiveUiContainer.MODAL_DIALOG;
        assertEquals(expectedMask, entry.getAllowedContainers());
    }

    @Test
    public void testBuilderFailsWhenNoContainers() {
        AdaptiveUiEntry.Builder builder =
                new AdaptiveUiEntry.Builder().setScope(AdaptiveUiScope.TRANSIENT);
        assertThrows(IllegalStateException.class, builder::build);
    }
}
