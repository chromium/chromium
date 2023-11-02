// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.legal_disclaimer;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the legal_disclaimer of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantLegalDisclaimerModel extends PropertyModel {
    static final WritableObjectPropertyKey<AssistantLegalDisclaimer> LEGAL_DISCLAIMER =
            new WritableObjectPropertyKey<>();

    public AssistantLegalDisclaimerModel() {
        super(LEGAL_DISCLAIMER);
    }

    /** The delegate and message can be set as null to clear the legal_disclaimer from the view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    public void setLegalDisclaimer(
            @Nullable AssistantLegalDisclaimerDelegate delegate, @Nullable String message) {
        if (delegate == null || TextUtils.isEmpty(message)) {
            set(LEGAL_DISCLAIMER, null);
            return;
        }
        set(LEGAL_DISCLAIMER, new AssistantLegalDisclaimer(delegate, message));
    }
}
