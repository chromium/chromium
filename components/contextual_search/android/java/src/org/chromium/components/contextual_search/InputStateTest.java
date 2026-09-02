// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.AimModelsProto.ModelMode;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

import java.util.Map;

/** Unit tests for {@link InputState}. */
@RunWith(BaseRobolectricTestRunner.class)
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
                        .withMaxInputsByType(
                                Map.of(InputType.INPUT_TYPE_LENS_IMAGE_VALUE, 3))
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

    @Test
    public void testLazyProtobufParsing_whenOptimizationsEnabled() {
        OmniboxFeatures.sModelPickerOptimizations.setForTesting(true);

        InputTypeConfig inputTypeConfig =
                InputTypeConfig.newBuilder()
                        .setInputTypeValue(InputType.INPUT_TYPE_LENS_IMAGE_VALUE)
                        .setMenuLabel("Lens Image")
                        .build();
        ToolConfig toolConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .setChipLabel("Deep Search Chip")
                        .build();
        SectionConfig toolsSectionConfig =
                SectionConfig.newBuilder().setHeader("Tools Header").build();
        ModelConfig modelConfig =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        SectionConfig modelSectionConfig =
                SectionConfig.newBuilder().setHeader("Models Header").build();

        InputState state =
                new InputState.Builder()
                        .withInputTypeConfigs(new byte[][] {inputTypeConfig.toByteArray()})
                        .withToolConfigs(new byte[][] {toolConfig.toByteArray()})
                        .withToolsSectionConfig(toolsSectionConfig.toByteArray())
                        .withModelConfigs(new byte[][] {modelConfig.toByteArray()})
                        .withModelSectionConfig(modelSectionConfig.toByteArray())
                        .build();

        // 1. Check deserialized values match expectations.
        assertEquals(1, state.getInputTypeConfigs().size());
        assertEquals("Lens Image", state.getInputTypeConfigs().get(0).getMenuLabel());

        assertEquals(1, state.getToolConfigs().size());
        assertEquals("Deep Search", state.getToolConfigs().get(0).getMenuLabel());

        assertEquals("Tools Header", state.getToolsSectionConfig().getHeader());

        assertEquals(1, state.getModelConfigs().size());
        assertEquals("Pro", state.getModelConfigs().get(0).getMenuLabel());

        assertEquals("Models Header", state.getModelSectionConfig().getHeader());

        // 2. Check memoization (subsequent calls return the exact same parsed instances).
        assertSame(state.getInputTypeConfigs(), state.getInputTypeConfigs());
        assertSame(state.getToolConfigs(), state.getToolConfigs());
        assertSame(state.getToolsSectionConfig(), state.getToolsSectionConfig());
        assertSame(state.getModelConfigs(), state.getModelConfigs());
        assertSame(state.getModelSectionConfig(), state.getModelSectionConfig());
    }

    @Test
    public void testEagerProtobufParsing_whenOptimizationsDisabled() {
        OmniboxFeatures.sModelPickerOptimizations.setForTesting(false);

        InputTypeConfig inputTypeConfig =
                InputTypeConfig.newBuilder()
                        .setInputTypeValue(InputType.INPUT_TYPE_LENS_IMAGE_VALUE)
                        .setMenuLabel("Lens Image")
                        .build();
        ToolConfig toolConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .build();
        SectionConfig toolsSectionConfig =
                SectionConfig.newBuilder().setHeader("Tools Header").build();
        ModelConfig modelConfig =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        SectionConfig modelSectionConfig =
                SectionConfig.newBuilder().setHeader("Models Header").build();

        InputState state =
                new InputState.Builder()
                        .withInputTypeConfigs(new byte[][] {inputTypeConfig.toByteArray()})
                        .withToolConfigs(new byte[][] {toolConfig.toByteArray()})
                        .withToolsSectionConfig(toolsSectionConfig.toByteArray())
                        .withModelConfigs(new byte[][] {modelConfig.toByteArray()})
                        .withModelSectionConfig(modelSectionConfig.toByteArray())
                        .build();

        assertEquals(1, state.getInputTypeConfigs().size());
        assertEquals("Lens Image", state.getInputTypeConfigs().get(0).getMenuLabel());
        assertEquals(1, state.getToolConfigs().size());
        assertEquals("Deep Search", state.getToolConfigs().get(0).getMenuLabel());
        assertEquals("Tools Header", state.getToolsSectionConfig().getHeader());
        assertEquals(1, state.getModelConfigs().size());
        assertEquals("Pro", state.getModelConfigs().get(0).getMenuLabel());
        assertEquals("Models Header", state.getModelSectionConfig().getHeader());
    }

    @Test
    public void testEmptyAndNullConfigs() {
        InputState state = new InputState.Builder().build();

        assertNotNull(state.getInputTypeConfigs());
        assertTrue(state.getInputTypeConfigs().isEmpty());

        assertNotNull(state.getToolConfigs());
        assertTrue(state.getToolConfigs().isEmpty());

        assertNotNull(state.getToolsSectionConfig());
        assertEquals(SectionConfig.getDefaultInstance(), state.getToolsSectionConfig());

        assertNotNull(state.getModelConfigs());
        assertTrue(state.getModelConfigs().isEmpty());

        assertNotNull(state.getModelSectionConfig());
        assertEquals(SectionConfig.getDefaultInstance(), state.getModelSectionConfig());

        // Test with null elements inside arrays
        InputState stateWithNullElements =
                new InputState.Builder()
                        .withInputTypeConfigs(new byte[][] {null})
                        .withToolConfigs(new byte[][] {null})
                        .withModelConfigs(new byte[][] {null})
                        .build();
        assertTrue(stateWithNullElements.getInputTypeConfigs().isEmpty());
        assertTrue(stateWithNullElements.getToolConfigs().isEmpty());
        assertTrue(stateWithNullElements.getModelConfigs().isEmpty());
    }

    @Test
    public void testInvalidProtoBytesGracefulFallback() {
        byte[] invalidBytes = new byte[] {(byte) 0xFF, (byte) 0xFF, (byte) 0xFF};
        InputState state =
                new InputState.Builder()
                        .withInputTypeConfigs(new byte[][] {invalidBytes})
                        .withToolConfigs(new byte[][] {invalidBytes})
                        .withToolsSectionConfig(invalidBytes)
                        .withModelConfigs(new byte[][] {invalidBytes})
                        .withModelSectionConfig(invalidBytes)
                        .build();

        assertTrue(state.getInputTypeConfigs().isEmpty());
        assertTrue(state.getToolConfigs().isEmpty());
        assertEquals(SectionConfig.getDefaultInstance(), state.getToolsSectionConfig());
        assertTrue(state.getModelConfigs().isEmpty());
        assertEquals(SectionConfig.getDefaultInstance(), state.getModelSectionConfig());
    }

    @Test
    public void testEqualsAndHashCode_lazyAndEager() {
        InputTypeConfig inputTypeConfig =
                InputTypeConfig.newBuilder()
                        .setInputTypeValue(InputType.INPUT_TYPE_LENS_IMAGE_VALUE)
                        .setMenuLabel("Lens Image")
                        .build();
        ToolConfig toolConfig =
                ToolConfig.newBuilder()
                        .setTool(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .build();
        SectionConfig toolsSectionConfig =
                SectionConfig.newBuilder().setHeader("Tools Header").build();
        ModelConfig modelConfig =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO_VALUE)
                        .setMenuLabel("Pro")
                        .build();
        SectionConfig modelSectionConfig =
                SectionConfig.newBuilder().setHeader("Models Header").build();

        OmniboxFeatures.sModelPickerOptimizations.setForTesting(true);
        InputState lazyState =
                new InputState.Builder()
                        .withInputTypeConfigs(new byte[][] {inputTypeConfig.toByteArray()})
                        .withToolConfigs(new byte[][] {toolConfig.toByteArray()})
                        .withToolsSectionConfig(toolsSectionConfig.toByteArray())
                        .withModelConfigs(new byte[][] {modelConfig.toByteArray()})
                        .withModelSectionConfig(modelSectionConfig.toByteArray())
                        .build();

        OmniboxFeatures.sModelPickerOptimizations.setForTesting(false);
        InputState eagerState =
                new InputState.Builder()
                        .withInputTypeConfigs(new byte[][] {inputTypeConfig.toByteArray()})
                        .withToolConfigs(new byte[][] {toolConfig.toByteArray()})
                        .withToolsSectionConfig(toolsSectionConfig.toByteArray())
                        .withModelConfigs(new byte[][] {modelConfig.toByteArray()})
                        .withModelSectionConfig(modelSectionConfig.toByteArray())
                        .build();

        assertEquals(lazyState, eagerState);
        assertEquals(eagerState, lazyState);
        assertEquals(lazyState.hashCode(), eagerState.hashCode());
    }
}
