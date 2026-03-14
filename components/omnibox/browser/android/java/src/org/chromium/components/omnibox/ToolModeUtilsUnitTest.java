// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

/** Tests for {@link ToolModeUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolModeUtilsUnitTest {
    @Test
    public void testIsAimRequest() {
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.AI_MODE));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.IMAGE_GENERATION));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.DEEP_SEARCH));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.CANVAS));

        assertFalse(ToolModeUtils.isAimRequest(AutocompleteRequestType.SEARCH));
        assertFalse(ToolModeUtils.isAimRequest(AutocompleteRequestType.SEARCH_PREFETCH));
    }

    @Test
    public void testIsConventionalRequest() {
        assertTrue(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.SEARCH));
        assertTrue(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.SEARCH_PREFETCH));

        assertFalse(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.AI_MODE));
        assertFalse(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.IMAGE_GENERATION));
        assertFalse(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.DEEP_SEARCH));
        assertFalse(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.CANVAS));
    }

    @Test
    public void testGetToolModeForRequestType() {
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.IMAGE_GENERATION, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.IMAGE_GENERATION, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.DEEP_SEARCH, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.DEEP_SEARCH, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_CANVAS_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.CANVAS, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_CANVAS_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.CANVAS, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.SEARCH, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.SEARCH_PREFETCH, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED_VALUE,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.AI_MODE, /* hasAttachments= */ false));
    }
}
