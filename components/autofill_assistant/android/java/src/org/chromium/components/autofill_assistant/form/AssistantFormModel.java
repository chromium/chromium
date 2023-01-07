// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.form;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.AssistantInfoPopup;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A model for the assistant form. */
@JNINamespace("autofill_assistant")
public class AssistantFormModel extends PropertyModel {
    public static final WritableObjectPropertyKey<String> INFO_LABEL =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantInfoPopup> INFO_POPUP =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AssistantFormInput>> INPUTS =
            new WritableObjectPropertyKey<>();

    public AssistantFormModel() {
        super(INFO_LABEL, INFO_POPUP, INPUTS);
    }

    @CalledByNative
    private void setInputs(List<AssistantFormInput> inputs) {
        set(INPUTS, inputs);
    }

    @CalledByNative
    private void setInfoPopup(AssistantInfoPopup infoPopup) {
        set(INFO_POPUP, infoPopup);
    }

    @CalledByNative
    private void clearInfoPopup() {
        set(INFO_POPUP, null);
    }

    @CalledByNative
    private void setInfoLabel(String label) {
        set(INFO_LABEL, label);
    }

    @CalledByNative
    private void clearInfoLabel() {
        set(INFO_LABEL, null);
    }

    @CalledByNative
    private void clearInputs() {
        set(INPUTS, Arrays.asList());
    }

    @CalledByNative
    private static List<AssistantFormInput> createInputList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addInput(List<AssistantFormInput> inputs, AssistantFormInput input) {
        inputs.add(input);
    }
}
