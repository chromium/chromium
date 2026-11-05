// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link AdaptiveUiEntry} and {@link AdaptiveUiProperties}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiEntryTest {

    @Test
    public void testBuilderSuccess() {
        PropertyModel model = new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS).build();
        AdaptiveUiEntry entry =
                new AdaptiveUiEntry.Builder(model)
                        .setScope(AdaptiveUiScope.WINDOW)
                        .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                        .addAllowedContainer(AdaptiveUiContainer.MODAL_DIALOG)
                        .build();

        assertEquals(AdaptiveUiScope.WINDOW, entry.getScope());
        int expectedMask = AdaptiveUiContainer.BOTTOM_SHEET | AdaptiveUiContainer.MODAL_DIALOG;
        assertEquals(expectedMask, entry.getAllowedContainers());
        assertSame(model, entry.getPropertyModel());
        assertNull(entry.getObserver());
    }

    @Test
    public void testBuilderSetsObserver() {
        PropertyModel model = new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS).build();
        AdaptiveUiObserver observer = reason -> {};
        AdaptiveUiEntry entry =
                new AdaptiveUiEntry.Builder(model)
                        .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                        .setObserver(observer)
                        .build();

        assertEquals(observer, entry.getObserver());
    }

    @Test
    public void testBuilderFailsWhenNoContainers() {
        PropertyModel model = new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS).build();
        AdaptiveUiEntry.Builder builder =
                new AdaptiveUiEntry.Builder(model).setScope(AdaptiveUiScope.TRANSIENT);
        assertThrows(IllegalStateException.class, builder::build);
    }

    @Test
    public void testAllKeysContainsEveryDeclaredKey() throws IllegalAccessException {
        List<PropertyKey> allKeys = Arrays.asList(AdaptiveUiProperties.ALL_KEYS);
        int declaredKeyCount = 0;
        for (Field field : AdaptiveUiProperties.class.getDeclaredFields()) {
            if (!PropertyKey.class.isAssignableFrom(field.getType())) continue;
            declaredKeyCount++;
            assertTrue(
                    field.getName() + " is missing from ALL_KEYS",
                    allKeys.contains(field.get(null)));
        }
        assertEquals(declaredKeyCount, allKeys.size());
    }
}
