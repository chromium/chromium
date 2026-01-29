// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.contextual_search;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
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
    public final int[] allowedTools;
    public final int[] allowedModels;
    public final int[] allowedInputTypes;
    public final int activeTool;
    public final int activeModel;
    public final int[] disabledTools;
    public final int[] disabledModels;
    public final int[] disabledInputTypes;

    @CalledByNative
    public InputState(
            @JniType("std::vector<omnibox::ToolMode>") int[] allowedTools,
            @JniType("std::vector<omnibox::ModelMode>") int[] allowedModels,
            @JniType("std::vector<omnibox::InputType>") int[] allowedInputTypes,
            @JniType("omnibox::ToolMode") int activeTool,
            @JniType("omnibox::ModelMode") int activeModel,
            @JniType("std::vector<omnibox::ToolMode>") int[] disabledTools,
            @JniType("std::vector<omnibox::ModelMode>") int[] disabledModels,
            @JniType("std::vector<omnibox::InputType>") int[] disabledInputTypes) {
        this.allowedTools = allowedTools;
        this.allowedModels = allowedModels;
        this.allowedInputTypes = allowedInputTypes;
        this.activeTool = activeTool;
        this.activeModel = activeModel;
        this.disabledTools = disabledTools;
        this.disabledModels = disabledModels;
        this.disabledInputTypes = disabledInputTypes;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof InputState)) return false;
        InputState that = (InputState) o;
        return activeTool == that.activeTool
                && activeModel == that.activeModel
                && Arrays.equals(allowedTools, that.allowedTools)
                && Arrays.equals(allowedModels, that.allowedModels)
                && Arrays.equals(allowedInputTypes, that.allowedInputTypes)
                && Arrays.equals(disabledTools, that.disabledTools)
                && Arrays.equals(disabledModels, that.disabledModels)
                && Arrays.equals(disabledInputTypes, that.disabledInputTypes);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                activeTool,
                activeModel,
                Arrays.hashCode(allowedTools),
                Arrays.hashCode(allowedModels),
                Arrays.hashCode(allowedInputTypes),
                Arrays.hashCode(disabledTools),
                Arrays.hashCode(disabledModels),
                Arrays.hashCode(disabledInputTypes));
    }
}
