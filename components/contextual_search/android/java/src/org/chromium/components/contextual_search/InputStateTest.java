// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

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
        String hintText = "hint1";
        int[] allowedInputTypes = {
            InputType.INPUT_TYPE_LENS_IMAGE_VALUE, InputType.INPUT_TYPE_BROWSER_TAB_VALUE
        };
        int[] disabledInputTypes = {InputType.INPUT_TYPE_BROWSER_TAB_VALUE};
        int maxTotalInputs = 16;
        int[] maxInstancesKeys = {InputType.INPUT_TYPE_LENS_IMAGE_VALUE};
        int[] maxInstancesValues = {3};
        byte[][] inputTypeConfigs = {InputTypeConfig.getDefaultInstance().toByteArray()};
        int activeTool = ToolMode.TOOL_MODE_IMAGE_GEN_VALUE;
        int[] allowedTools = {
            ToolMode.TOOL_MODE_DEEP_SEARCH_VALUE, ToolMode.TOOL_MODE_CANVAS_VALUE
        };
        int[] disabledTools = {ToolMode.TOOL_MODE_CANVAS_VALUE};
        boolean imageGenUploadActive = true;
        byte[][] toolConfigs = {ToolConfig.getDefaultInstance().toByteArray()};
        byte[] toolsSectionConfig = SectionConfig.getDefaultInstance().toByteArray();
        int activeModel = ModelMode.MODEL_MODE_GEMINI_PRO_VALUE;
        int defaultModel = ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE;
        int[] allowedModels = {
            ModelMode.MODEL_MODE_GEMINI_PRO_VALUE, ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE
        };
        int[] disabledModels = {ModelMode.MODEL_MODE_GEMINI_REGULAR_VALUE};
        byte[][] modelConfigs = {ModelConfig.getDefaultInstance().toByteArray()};
        byte[] modelSectionConfig = SectionConfig.getDefaultInstance().toByteArray();
        InputState state1 =
                new InputState(
                        hintText,
                        allowedInputTypes,
                        disabledInputTypes,
                        maxTotalInputs,
                        maxInstancesKeys,
                        maxInstancesValues,
                        inputTypeConfigs,
                        activeTool,
                        allowedTools,
                        disabledTools,
                        imageGenUploadActive,
                        toolConfigs,
                        toolsSectionConfig,
                        activeModel,
                        defaultModel,
                        allowedModels,
                        disabledModels,
                        modelConfigs,
                        modelSectionConfig);
        InputState state2 =
                new InputState(
                        hintText,
                        allowedInputTypes,
                        disabledInputTypes,
                        maxTotalInputs,
                        maxInstancesKeys,
                        maxInstancesValues,
                        inputTypeConfigs,
                        activeTool,
                        allowedTools,
                        disabledTools,
                        imageGenUploadActive,
                        toolConfigs,
                        toolsSectionConfig,
                        activeModel,
                        defaultModel,
                        allowedModels,
                        disabledModels,
                        modelConfigs,
                        modelSectionConfig);

        assertEquals(state1, state2);
        assertEquals(state1.hashCode(), state2.hashCode());

        byte[][] diffToolConfigs = {
            ToolConfig.newBuilder().setMenuLabel("diff").build().toByteArray()
        };
        InputState state3 =
                new InputState(
                        hintText,
                        allowedInputTypes,
                        disabledInputTypes,
                        maxTotalInputs,
                        maxInstancesKeys,
                        maxInstancesValues,
                        inputTypeConfigs,
                        activeTool,
                        allowedTools,
                        disabledTools,
                        imageGenUploadActive,
                        // This arg is the only difference.
                        diffToolConfigs,
                        toolsSectionConfig,
                        activeModel,
                        defaultModel,
                        allowedModels,
                        disabledModels,
                        modelConfigs,
                        modelSectionConfig);

        assertNotEquals(state1, state3);
        assertNotEquals(state1.hashCode(), state3.hashCode());
    }
}
