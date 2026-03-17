// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.AimModelsProto.ModelMode;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

/** Unit tests for {@link InputState}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InputStateTest {
    @Test
    public void testEqualsAndHashCode() {
        InputState.Builder builder =
                new InputState.Builder()
                        .withHintText("hint1")
                        .withAllowedInputTypes(
                                InputType.INPUT_TYPE_LENS_IMAGE_VALUE,
                                InputType.INPUT_TYPE_BROWSER_TAB_VALUE)
                        .withDisabledInputTypes(InputType.INPUT_TYPE_BROWSER_TAB_VALUE)
                        .withMaxTotalInputs(16)
                        .withMaxInstances(
                                new int[] {InputType.INPUT_TYPE_LENS_IMAGE_VALUE}, new int[] {3})
                        .withInputTypeConfigs(
                                new byte[][] {InputTypeConfig.getDefaultInstance().toByteArray()})
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withAllowedTools(
                                ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE,
                                ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_CANVAS_VALUE)
                        .withImageGenUploadActive(true)
                        .withToolConfigs(
                                new byte[][] {ToolConfig.getDefaultInstance().toByteArray()})
                        .withToolsSectionConfig(SectionConfig.getDefaultInstance().toByteArray())
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO_VALUE,
                                ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE)
                        .withModelConfigs(
                                new byte[][] {ModelConfig.getDefaultInstance().toByteArray()})
                        .withModelSectionConfig(SectionConfig.getDefaultInstance().toByteArray());

        InputState state1 = builder.build();
        InputState state2 = builder.build();

        assertEquals(state1, state2);
        assertEquals(state1.hashCode(), state2.hashCode());

        byte[][] diffToolConfigs = {
            ToolConfig.newBuilder().setMenuLabel("diff").build().toByteArray()
        };
        InputState state3 = builder.withToolConfigs(diffToolConfigs).build();

        assertNotEquals(state1, state3);
        assertNotEquals(state1.hashCode(), state3.hashCode());
    }

    @Test
    public void testVisibilityAndEnablement() {
        InputState state =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .withAllowedModels(ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE)
                        .build();

        assertTrue(state.isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
        assertTrue(state.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        assertTrue(state.isToolVisible(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE));
        assertFalse(state.isToolEnabled(ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE));

        assertFalse(state.isToolVisible(ToolMode.TOOL_MODE_CANVAS_VALUE));
        assertFalse(state.isToolEnabled(ToolMode.TOOL_MODE_CANVAS_VALUE));

        assertTrue(state.isModelVisible(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE));
        assertTrue(state.isModelEnabled(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE));

        assertTrue(state.isModelVisible(ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE));
        assertFalse(state.isModelEnabled(ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE));

        assertFalse(state.isModelVisible(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE));
        assertFalse(state.isModelEnabled(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE_VALUE));
    }

    @Test
    public void testIsToolEnabled() {
        InputState activeAllowedDisabled =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .build();
        assertTrue(activeAllowedDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState activeNotAllowedDisabled =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .build();
        assertTrue(activeNotAllowedDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState allowedNotDisabled =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .build();
        assertTrue(allowedNotDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState allowedDisabled =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .build();
        assertFalse(allowedDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));

        InputState notAllowedNotDisabled = new InputState.Builder().build();
        assertFalse(notAllowedNotDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE));
    }

    @Test
    public void testEitherImageGenToolVisibilityAndEnablement() {
        InputState state =
                new InputState.Builder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .build();

        assertTrue(state.isImageGenToolVisible());
        assertTrue(state.isImageGenToolEnabled());

        InputState stateOnlyUploadAllowed =
                new InputState.Builder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .build();
        assertTrue(stateOnlyUploadAllowed.isImageGenToolVisible());
        assertTrue(stateOnlyUploadAllowed.isImageGenToolEnabled());

        InputState stateNeitherVisible = new InputState.Builder().build();
        assertFalse(stateNeitherVisible.isImageGenToolVisible());
        assertFalse(stateNeitherVisible.isImageGenToolEnabled());

        InputState stateBothDisabled =
                new InputState.Builder()
                        .withAllowedTools(
                                ToolMode.TOOL_MODE_IMAGE_GEN_VALUE,
                                ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .withDisabledTools(
                                ToolMode.TOOL_MODE_IMAGE_GEN_VALUE,
                                ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE)
                        .build();
        assertTrue(stateBothDisabled.isImageGenToolVisible());
        assertFalse(stateBothDisabled.isImageGenToolEnabled());
    }
}
