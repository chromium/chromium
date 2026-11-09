// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;

/** Tests for {@link ToolModeUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolModeUtilsUnitTest {
    @Test
    public void testIsAimRequest() {
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.AI_MODE));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.IMAGE_GENERATION));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.DEEP_SEARCH));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.CANVAS));
        assertTrue(ToolModeUtils.isAimRequest(AutocompleteRequestType.PTM));

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
        assertFalse(ToolModeUtils.isConventionalRequest(AutocompleteRequestType.PTM));
    }

    @Test
    public void testGetToolModeForRequestType() {
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.IMAGE_GENERATION, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.IMAGE_GENERATION, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_DEEP_SEARCH,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.DEEP_SEARCH, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_DEEP_SEARCH,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.DEEP_SEARCH, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_CANVAS,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.CANVAS, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_CANVAS,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.CANVAS, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_GEMINI_PRO,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.PTM, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_GEMINI_PRO,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.PTM, /* hasAttachments= */ true));

        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.SEARCH, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.SEARCH_PREFETCH, /* hasAttachments= */ false));
        assertEquals(
                ToolMode.TOOL_MODE_UNSPECIFIED,
                ToolModeUtils.getToolModeForRequestType(
                        AutocompleteRequestType.AI_MODE, /* hasAttachments= */ false));
    }

    @Test
    public void testGetRequestTypeForToolMode() {
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                ToolModeUtils.getRequestTypeForToolMode(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertEquals(
                AutocompleteRequestType.IMAGE_GENERATION,
                ToolModeUtils.getRequestTypeForToolMode(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD));
        assertEquals(
                AutocompleteRequestType.DEEP_SEARCH,
                ToolModeUtils.getRequestTypeForToolMode(ToolMode.TOOL_MODE_DEEP_SEARCH));
        assertEquals(
                AutocompleteRequestType.CANVAS,
                ToolModeUtils.getRequestTypeForToolMode(ToolMode.TOOL_MODE_CANVAS));
        assertEquals(
                AutocompleteRequestType.PTM,
                ToolModeUtils.getRequestTypeForToolMode(ToolMode.TOOL_MODE_GEMINI_PRO));
        assertEquals(
                AutocompleteRequestType.AI_MODE,
                ToolModeUtils.getRequestTypeForToolMode(ToolMode.TOOL_MODE_UNSPECIFIED));
    }
}
