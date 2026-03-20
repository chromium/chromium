// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.SectionConfigProto.SectionConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * A Java side copy of the same named object in C++. Separated from the model object, this is a
 * constant snapshot of state, and is to be replaced and not updated. Represents allowed, disabled,
 * and active tools, models, and inputs. Allows a data driven approach for the UI to dynamically
 * update instead of locally hard coded rules.
 */
@JNINamespace("contextual_search")
@NullMarked
public class InputState {
    private static final String TAG = "InputState";

    public final String hintText;

    public final List<Integer> allowedInputTypes;
    public final List<Integer> disabledInputTypes;
    public final int maxTotalInputs;
    public final List<Integer> maxInstancesKeys;
    public final List<Integer> maxInstancesValues;
    public final List<InputTypeConfig> inputTypeConfigs;

    public final int activeTool;
    public final List<Integer> allowedTools;
    public final List<Integer> disabledTools;
    public final boolean imageGenUploadActive;
    public final List<ToolConfig> toolConfigs;
    public final SectionConfig toolsSectionConfig;

    public final int activeModel;
    public final int defaultModel;
    public final List<Integer> allowedModels;
    public final List<Integer> disabledModels;
    public final List<ModelConfig> modelConfigs;
    public final SectionConfig modelSectionConfig;

    // This constructor is extremely large and verbose. It's only acceptable right now because there
    // is a single usage of this class, and then a small test file. If we find that multiple clients
    // need to call this constructor, especially with default arguments, we should strongly consider
    // a builder pattern and/or test default to make it more ergonomic.
    @CalledByNative
    public InputState(
            @JniType("std::string") String hintText,
            @JniType("std::vector<omnibox::InputType>") int[] allowedInputTypes,
            @JniType("std::vector<omnibox::InputType>") int[] disabledInputTypes,
            int maxTotalInputs,
            @JniType("std::vector<omnibox::InputType>") int[] maxInstancesKeys,
            @JniType("std::vector<int>") int[] maxInstancesValues,
            byte @Nullable [][] inputTypeConfigs,
            @JniType("omnibox::ToolMode") int activeTool,
            @JniType("std::vector<omnibox::ToolMode>") int[] allowedTools,
            @JniType("std::vector<omnibox::ToolMode>") int[] disabledTools,
            boolean imageGenUploadActive,
            byte @Nullable [][] toolConfigs,
            @JniType("std::vector<uint8_t>") byte @Nullable [] toolsSectionConfig,
            @JniType("omnibox::ModelMode") int activeModel,
            @JniType("omnibox::ModelMode") int defaultModel,
            @JniType("std::vector<omnibox::ModelMode>") int[] allowedModels,
            @JniType("std::vector<omnibox::ModelMode>") int[] disabledModels,
            byte @Nullable [][] modelConfigs,
            @JniType("std::vector<uint8_t>") byte @Nullable [] modelSectionConfig) {
        this.hintText = hintText;

        this.allowedInputTypes = toList(allowedInputTypes);
        this.disabledInputTypes = toList(disabledInputTypes);
        this.maxTotalInputs = maxTotalInputs;
        this.maxInstancesKeys = toList(maxInstancesKeys);
        this.maxInstancesValues = toList(maxInstancesValues);
        this.inputTypeConfigs = parseInputTypeConfigs(inputTypeConfigs);

        this.activeTool = activeTool;
        this.allowedTools = toList(allowedTools);
        this.disabledTools = toList(disabledTools);
        this.imageGenUploadActive = imageGenUploadActive;
        this.toolConfigs = parseToolConfigs(toolConfigs);
        this.toolsSectionConfig = parseSectionConfig(toolsSectionConfig);

        this.activeModel = activeModel;
        this.defaultModel = defaultModel;
        this.allowedModels = toList(allowedModels);
        this.disabledModels = toList(disabledModels);
        this.modelConfigs = parseModelConfigs(modelConfigs);
        this.modelSectionConfig = parseSectionConfig(modelSectionConfig);
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof InputState)) return false;
        InputState that = (InputState) o;
        return Objects.equals(hintText, that.hintText)
                && maxTotalInputs == that.maxTotalInputs
                && Objects.equals(allowedInputTypes, that.allowedInputTypes)
                && Objects.equals(disabledInputTypes, that.disabledInputTypes)
                && Objects.equals(maxInstancesKeys, that.maxInstancesKeys)
                && Objects.equals(maxInstancesValues, that.maxInstancesValues)
                && Objects.equals(inputTypeConfigs, that.inputTypeConfigs)
                && activeTool == that.activeTool
                && Objects.equals(allowedTools, that.allowedTools)
                && Objects.equals(disabledTools, that.disabledTools)
                && imageGenUploadActive == that.imageGenUploadActive
                && Objects.equals(toolConfigs, that.toolConfigs)
                && Objects.equals(toolsSectionConfig, that.toolsSectionConfig)
                && activeModel == that.activeModel
                && defaultModel == that.defaultModel
                && Objects.equals(allowedModels, that.allowedModels)
                && Objects.equals(disabledModels, that.disabledModels)
                && Objects.equals(modelConfigs, that.modelConfigs)
                && Objects.equals(modelSectionConfig, that.modelSectionConfig);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                hintText,
                maxTotalInputs,
                allowedInputTypes,
                disabledInputTypes,
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
    }

    /**
     * @param toolMode The tool mode to check.
     * @return Whether the tool should be visible in the UI.
     */
    public boolean isToolVisible(int toolMode) {
        return activeTool == toolMode || allowedTools.contains(toolMode);
    }

    /**
     * @param toolMode The tool mode to check.
     * @return Whether the tool should be enabled in the UI.
     */
    public boolean isToolEnabled(int toolMode) {
        return activeTool == toolMode
                || (allowedTools.contains(toolMode) && !disabledTools.contains(toolMode));
    }

    /** Returns whether the image gen tool should be visible, by checking both tool modes. */
    public boolean isImageGenToolVisible() {
        return isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                || isToolVisible(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE);
    }

    /** Returns whether the image gen tool should be enabled, by checking both tool modes. */
    public boolean isImageGenToolEnabled() {
        return isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE)
                || isToolEnabled(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE);
    }

    /**
     * @param modelMode The model mode to check.
     * @return Whether the model should be visible in the UI.
     */
    public boolean isModelVisible(int modelMode) {
        return activeModel == modelMode || allowedModels.contains(modelMode);
    }

    /**
     * @param modelMode The model mode to check.
     * @return Whether the model should be enabled in the UI.
     */
    public boolean isModelEnabled(int modelMode) {
        return activeModel == modelMode
                || (allowedModels.contains(modelMode) && !disabledModels.contains(modelMode));
    }

    private static List<Integer> toList(int @Nullable [] array) {
        if (array == null) return Collections.emptyList();
        List<Integer> list = new ArrayList<>(array.length);
        for (int v : array) list.add(v);
        return Collections.unmodifiableList(list);
    }

    private static List<InputTypeConfig> parseInputTypeConfigs(byte @Nullable [][] configs) {
        if (configs == null) return Collections.emptyList();
        List<InputTypeConfig> result = new ArrayList<>(configs.length);
        for (byte[] config : configs) {
            try {
                result.add(InputTypeConfig.parseFrom(config));
            } catch (InvalidProtocolBufferException e) {
                Log.e(TAG, "Failed to parse InputTypeConfig", e);
            }
        }
        return Collections.unmodifiableList(result);
    }

    private static List<ToolConfig> parseToolConfigs(byte @Nullable [][] configs) {
        if (configs == null) return Collections.emptyList();
        List<ToolConfig> result = new ArrayList<>(configs.length);
        for (byte[] config : configs) {
            try {
                result.add(ToolConfig.parseFrom(config));
            } catch (InvalidProtocolBufferException e) {
                Log.e(TAG, "Failed to parse ToolConfig", e);
            }
        }
        return Collections.unmodifiableList(result);
    }

    private static List<ModelConfig> parseModelConfigs(byte @Nullable [][] configs) {
        if (configs == null) return Collections.emptyList();
        List<ModelConfig> result = new ArrayList<>(configs.length);
        for (byte[] config : configs) {
            try {
                result.add(ModelConfig.parseFrom(config));
            } catch (InvalidProtocolBufferException e) {
                Log.e(TAG, "Failed to parse ModelConfig", e);
            }
        }
        return Collections.unmodifiableList(result);
    }

    private static SectionConfig parseSectionConfig(byte @Nullable [] config) {
        if (config == null) return SectionConfig.getDefaultInstance();
        try {
            return SectionConfig.parseFrom(config);
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Failed to parse SectionConfig", e);
            return SectionConfig.getDefaultInstance();
        }
    }

    public static class Builder {
        private String mHintText = "";
        private int[] mAllowedInputTypes = new int[0];
        private int[] mDisabledInputTypes = new int[0];
        private int mMaxTotalInputs;
        private int[] mMaxInstancesKeys = new int[0];
        private int[] mMaxInstancesValues = new int[0];
        private byte @Nullable [][] mInputTypeConfigs;
        private int mActiveTool;
        private int[] mAllowedTools = new int[0];
        private int[] mDisabledTools = new int[0];
        private boolean mImageGenUploadActive;
        private byte @Nullable [][] mToolConfigs;
        private byte @Nullable [] mToolsSectionConfig;
        private int mActiveModel;
        private int mDefaultModel;
        private int[] mAllowedModels = new int[0];
        private int[] mDisabledModels = new int[0];
        private byte @Nullable [][] mModelConfigs;
        private byte @Nullable [] mModelSectionConfig;

        public Builder withHintText(String hintText) {
            mHintText = hintText;
            return this;
        }

        public Builder withAllowedInputTypes(int... allowedInputTypes) {
            mAllowedInputTypes = allowedInputTypes;
            return this;
        }

        public Builder withDisabledInputTypes(int... disabledInputTypes) {
            mDisabledInputTypes = disabledInputTypes;
            return this;
        }

        public Builder withMaxTotalInputs(int maxTotalInputs) {
            mMaxTotalInputs = maxTotalInputs;
            return this;
        }

        public Builder withMaxInstances(int[] keys, int[] values) {
            assert keys.length == values.length;
            mMaxInstancesKeys = keys;
            mMaxInstancesValues = values;
            return this;
        }

        public Builder withInputTypeConfigs(byte[][] inputTypeConfigs) {
            mInputTypeConfigs = inputTypeConfigs;
            return this;
        }

        public Builder withActiveTool(int activeTool) {
            mActiveTool = activeTool;
            return this;
        }

        public Builder withAllowedTools(int... allowedTools) {
            mAllowedTools = allowedTools;
            return this;
        }

        public Builder withDisabledTools(int... disabledTools) {
            mDisabledTools = disabledTools;
            return this;
        }

        public Builder withImageGenUploadActive(boolean imageGenUploadActive) {
            mImageGenUploadActive = imageGenUploadActive;
            return this;
        }

        public Builder withToolConfigs(byte[][] toolConfigs) {
            mToolConfigs = toolConfigs;
            return this;
        }

        public Builder withToolsSectionConfig(byte[] toolsSectionConfig) {
            mToolsSectionConfig = toolsSectionConfig;
            return this;
        }

        public Builder withActiveModel(int activeModel) {
            mActiveModel = activeModel;
            return this;
        }

        public Builder withDefaultModel(int defaultModel) {
            mDefaultModel = defaultModel;
            return this;
        }

        public Builder withAllowedModels(int... allowedModels) {
            mAllowedModels = allowedModels;
            return this;
        }

        public Builder withDisabledModels(int... disabledModels) {
            mDisabledModels = disabledModels;
            return this;
        }

        public Builder withModelConfigs(byte[][] modelConfigs) {
            mModelConfigs = modelConfigs;
            return this;
        }

        public Builder withModelSectionConfig(byte[] modelSectionConfig) {
            mModelSectionConfig = modelSectionConfig;
            return this;
        }

        public InputState build() {
            return new InputState(
                    mHintText,
                    mAllowedInputTypes,
                    mDisabledInputTypes,
                    mMaxTotalInputs,
                    mMaxInstancesKeys,
                    mMaxInstancesValues,
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
}
