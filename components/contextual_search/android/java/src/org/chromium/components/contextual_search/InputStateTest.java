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
import org.chromium.components.omnibox.AimModelsProtoIntDef.ModelMode;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;

import java.util.Map;

/** Unit tests for {@link InputState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InputStateTest {
    @Test
    public void testEqualsAndHashCode() {
        InputStateBuilder builder =
                new InputStateBuilder()
                        .withHintText("hint1")
                        .withAllowedInputTypes(
                                InputType.INPUT_TYPE_LENS_IMAGE_VALUE,
                                InputType.INPUT_TYPE_BROWSER_TAB_VALUE)
                        .withDisabledInputTypes(InputType.INPUT_TYPE_BROWSER_TAB_VALUE)
                        .withMaxTotalInputs(16)
                        .withMaxInputsByType(Map.of(InputType.INPUT_TYPE_LENS_IMAGE_VALUE, 3))
                        .withInputTypeConfigs(
                                new byte[][] {InputTypeConfig.getDefaultInstance().toByteArray()})
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH, ToolMode.TOOL_MODE_CANVAS)
                        .withDisabledTools(ToolMode.TOOL_MODE_CANVAS)
                        .withImageGenUploadActive(true)
                        .withToolConfigs(
                                new byte[][] {ToolConfig.getDefaultInstance().toByteArray()})
                        .withToolsSectionConfig(SectionConfig.getDefaultInstance().toByteArray())
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withDefaultModel(ModelMode.MODEL_MODE_GEMINI_REGULAR)
                        .withAllowedModels(
                                ModelMode.MODEL_MODE_GEMINI_PRO,
                                ModelMode.MODEL_MODE_GEMINI_REGULAR)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_REGULAR)
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
                new InputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withAllowedTools(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .withDisabledTools(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .withActiveModel(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .withAllowedModels(ModelMode.MODEL_MODE_GEMINI_REGULAR)
                        .withDisabledModels(ModelMode.MODEL_MODE_GEMINI_REGULAR)
                        .build();

        assertTrue(state.isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN));
        assertTrue(state.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        assertTrue(state.isToolVisible(ToolMode.TOOL_MODE_DEEP_SEARCH));
        assertFalse(state.isToolEnabled(ToolMode.TOOL_MODE_DEEP_SEARCH));

        assertFalse(state.isToolVisible(ToolMode.TOOL_MODE_CANVAS));
        assertFalse(state.isToolEnabled(ToolMode.TOOL_MODE_CANVAS));

        assertTrue(state.isModelVisible(ModelMode.MODEL_MODE_GEMINI_PRO));
        assertTrue(state.isModelEnabled(ModelMode.MODEL_MODE_GEMINI_PRO));

        assertTrue(state.isModelVisible(ModelMode.MODEL_MODE_GEMINI_REGULAR));
        assertFalse(state.isModelEnabled(ModelMode.MODEL_MODE_GEMINI_REGULAR));

        assertFalse(state.isModelVisible(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE));
        assertFalse(state.isModelEnabled(ModelMode.MODEL_MODE_GEMINI_PRO_AUTOROUTE));
    }

    @Test
    public void testIsToolEnabled() {
        InputState activeAllowedDisabled =
                new InputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .build();
        assertTrue(activeAllowedDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        InputState activeNotAllowedDisabled =
                new InputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .build();
        assertTrue(activeNotAllowedDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        InputState allowedNotDisabled =
                new InputStateBuilder().withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN).build();
        assertTrue(allowedNotDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        InputState allowedDisabled =
                new InputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .build();
        assertFalse(allowedDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));

        InputState notAllowedNotDisabled = new InputStateBuilder().build();
        assertFalse(notAllowedNotDisabled.isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN));
    }

    @Test
    public void testEitherImageGenToolVisibilityAndEnablement() {
        InputState state =
                new InputStateBuilder()
                        .withActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN)
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .withDisabledTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .build();

        assertTrue(state.isImageGenToolVisible());
        assertTrue(state.isImageGenToolEnabled());

        InputState stateOnlyUploadAllowed =
                new InputStateBuilder()
                        .withAllowedTools(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .build();
        assertTrue(stateOnlyUploadAllowed.isImageGenToolVisible());
        assertTrue(stateOnlyUploadAllowed.isImageGenToolEnabled());

        InputState stateNeitherVisible = new InputStateBuilder().build();
        assertFalse(stateNeitherVisible.isImageGenToolVisible());
        assertFalse(stateNeitherVisible.isImageGenToolEnabled());

        InputState stateBothDisabled =
                new InputStateBuilder()
                        .withAllowedTools(
                                ToolMode.TOOL_MODE_IMAGE_GEN, ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .withDisabledTools(
                                ToolMode.TOOL_MODE_IMAGE_GEN, ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD)
                        .build();
        assertTrue(stateBothDisabled.isImageGenToolVisible());
        assertFalse(stateBothDisabled.isImageGenToolEnabled());
    }

    @Test
    public void testLazyProtobufParsing() {
        InputTypeConfig inputTypeConfig =
                InputTypeConfig.newBuilder()
                        .setInputTypeValue(InputType.INPUT_TYPE_LENS_IMAGE_VALUE)
                        .setMenuLabel("Lens Image")
                        .build();
        ToolConfig toolConfig =
                ToolConfig.newBuilder()
                        .setToolValue(ToolMode.TOOL_MODE_DEEP_SEARCH)
                        .setMenuLabel("Deep Search")
                        .setChipLabel("Deep Search Chip")
                        .build();
        SectionConfig toolsSectionConfig =
                SectionConfig.newBuilder().setHeader("Tools Header").build();
        ModelConfig modelConfig =
                ModelConfig.newBuilder()
                        .setModelValue(ModelMode.MODEL_MODE_GEMINI_PRO)
                        .setMenuLabel("Pro")
                        .build();
        SectionConfig modelSectionConfig =
                SectionConfig.newBuilder().setHeader("Models Header").build();

        InputState state =
                new InputStateBuilder()
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
    public void testEmptyAndNullConfigs() {
        InputState state = new InputStateBuilder().build();

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
                new InputStateBuilder()
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
                new InputStateBuilder()
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
}
