// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.AimModelsProtoIntDef.ModelMode;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;

import java.util.Collection;
import java.util.Collections;
import java.util.Map;

/** Test-support builder for creating {@link InputState} instances in unit tests. */
@NullMarked
public class InputStateBuilder {
    private String mHintText = "";
    private int[] mAllowedInputTypes = new int[0];
    private int[] mDisabledInputTypes = new int[0];
    private int mMaxTotalInputs;
    private Map<Integer, Integer> mMaxInputsByType = Collections.emptyMap();
    private byte @Nullable [][] mInputTypeConfigs;
    private @ToolMode int mActiveTool;
    private @ToolMode int[] mAllowedTools = new int[0];
    private @ToolMode int[] mDisabledTools = new int[0];
    private boolean mImageGenUploadActive;
    private byte @Nullable [][] mToolConfigs;
    private byte @Nullable [] mToolsSectionConfig;
    private @ModelMode int mActiveModel;
    private @ModelMode int mDefaultModel;
    private @ModelMode int[] mAllowedModels = new int[0];
    private @ModelMode int[] mDisabledModels = new int[0];
    private byte @Nullable [][] mModelConfigs;
    private byte @Nullable [] mModelSectionConfig;

    public InputStateBuilder() {}

    public InputStateBuilder(InputState other) {
        mHintText = other.hintText;
        mAllowedInputTypes = toIntArray(other.allowedInputTypes);
        mDisabledInputTypes = toIntArray(other.disabledInputTypes);
        mMaxTotalInputs = other.maxTotalInputs;
        mMaxInputsByType = other.maxInputsByType;
        withInputTypeConfigs(other.getInputTypeConfigs().toArray(new InputTypeConfig[0]));
        mActiveTool = other.activeTool;
        mAllowedTools = toIntArray(other.allowedTools);
        mDisabledTools = toIntArray(other.disabledTools);
        mImageGenUploadActive = other.imageGenUploadActive;
        withToolConfigs(other.getToolConfigs().toArray(new ToolConfig[0]));
        withToolsSectionConfig(other.getToolsSectionConfig());
        mActiveModel = other.activeModel;
        mDefaultModel = other.defaultModel;
        mAllowedModels = toIntArray(other.allowedModels);
        mDisabledModels = toIntArray(other.disabledModels);
        withModelConfigs(other.getModelConfigs().toArray(new ModelConfig[0]));
        withModelSectionConfig(other.getModelSectionConfig());
    }

    private static int[] toIntArray(Collection<Integer> collection) {
        int[] array = new int[collection.size()];
        int i = 0;
        for (int val : collection) {
            array[i++] = val;
        }
        return array;
    }

    public InputStateBuilder withHintText(String hintText) {
        mHintText = hintText;
        return this;
    }

    public InputStateBuilder withAllowedInputTypes(int... allowedInputTypes) {
        mAllowedInputTypes = allowedInputTypes;
        return this;
    }

    public InputStateBuilder withDisabledInputTypes(int... disabledInputTypes) {
        mDisabledInputTypes = disabledInputTypes;
        return this;
    }

    public InputStateBuilder withMaxTotalInputs(int maxTotalInputs) {
        mMaxTotalInputs = maxTotalInputs;
        return this;
    }

    public InputStateBuilder withMaxInputsByType(Map<Integer, Integer> maxInputsByType) {
        mMaxInputsByType = maxInputsByType;
        return this;
    }

    public InputStateBuilder withInputTypeConfigs(byte @Nullable [][] inputTypeConfigs) {
        mInputTypeConfigs = inputTypeConfigs;
        return this;
    }

    public InputStateBuilder withInputTypeConfigs(InputTypeConfig... inputTypeConfigs) {
        byte[][] rawConfigs = new byte[inputTypeConfigs.length][];
        for (int i = 0; i < inputTypeConfigs.length; i++) {
            rawConfigs[i] = inputTypeConfigs[i].toByteArray();
        }
        return withInputTypeConfigs(rawConfigs);
    }

    public InputStateBuilder withActiveTool(@ToolMode int activeTool) {
        mActiveTool = activeTool;
        return this;
    }

    public InputStateBuilder withAllowedTools(@ToolMode int... allowedTools) {
        mAllowedTools = allowedTools;
        return this;
    }

    public InputStateBuilder withDisabledTools(@ToolMode int... disabledTools) {
        mDisabledTools = disabledTools;
        return this;
    }

    public InputStateBuilder withImageGenUploadActive(boolean imageGenUploadActive) {
        mImageGenUploadActive = imageGenUploadActive;
        return this;
    }

    public InputStateBuilder withToolConfigs(byte @Nullable [][] toolConfigs) {
        mToolConfigs = toolConfigs;
        return this;
    }

    public InputStateBuilder withToolConfigs(ToolConfig... toolConfigs) {
        byte[][] rawConfigs = new byte[toolConfigs.length][];
        for (int i = 0; i < toolConfigs.length; i++) {
            rawConfigs[i] = toolConfigs[i].toByteArray();
        }
        return withToolConfigs(rawConfigs);
    }

    public InputStateBuilder withToolsSectionConfig(byte @Nullable [] toolsSectionConfig) {
        mToolsSectionConfig = toolsSectionConfig;
        return this;
    }

    public InputStateBuilder withToolsSectionConfig(SectionConfig toolsSectionConfig) {
        return withToolsSectionConfig(toolsSectionConfig.toByteArray());
    }

    public InputStateBuilder withActiveModel(@ModelMode int activeModel) {
        mActiveModel = activeModel;
        return this;
    }

    public InputStateBuilder withDefaultModel(@ModelMode int defaultModel) {
        mDefaultModel = defaultModel;
        return this;
    }

    public InputStateBuilder withAllowedModels(@ModelMode int... allowedModels) {
        mAllowedModels = allowedModels;
        return this;
    }

    public InputStateBuilder withDisabledModels(@ModelMode int... disabledModels) {
        mDisabledModels = disabledModels;
        return this;
    }

    public InputStateBuilder withModelConfigs(byte @Nullable [][] modelConfigs) {
        mModelConfigs = modelConfigs;
        return this;
    }

    public InputStateBuilder withModelConfigs(ModelConfig... modelConfigs) {
        byte[][] rawConfigs = new byte[modelConfigs.length][];
        for (int i = 0; i < modelConfigs.length; i++) {
            rawConfigs[i] = modelConfigs[i].toByteArray();
        }
        return withModelConfigs(rawConfigs);
    }

    public InputStateBuilder withModelSectionConfig(byte @Nullable [] modelSectionConfig) {
        mModelSectionConfig = modelSectionConfig;
        return this;
    }

    public InputStateBuilder withModelSectionConfig(SectionConfig modelSectionConfig) {
        return withModelSectionConfig(modelSectionConfig.toByteArray());
    }

    public InputState build() {
        return new InputState(
                mHintText,
                mAllowedInputTypes,
                mDisabledInputTypes,
                mMaxTotalInputs,
                mMaxInputsByType,
                mInputTypeConfigs,
                mActiveTool,
                mAllowedTools,
                mDisabledTools,
                mImageGenUploadActive,
                mToolConfigs,
                mToolsSectionConfig,
                mActiveModel,
                mDefaultModel,
                mAllowedModels,
                mDisabledModels,
                mModelConfigs,
                mModelSectionConfig);
    }
}
