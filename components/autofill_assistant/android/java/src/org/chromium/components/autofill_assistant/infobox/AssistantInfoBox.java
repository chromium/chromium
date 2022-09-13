// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.infobox;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.generic_ui.AssistantDrawable;

/** Java side equivalent of autofill_assistant::InfoBoxProto. */
@JNINamespace("autofill_assistant")
public class AssistantInfoBox {
    @Nullable
    private final AssistantDrawable mDrawable;
    private final String mExplanation;
    private final boolean mUseIntrinsicDimensions;

    public AssistantInfoBox(@Nullable AssistantDrawable drawable, String explanation,
            boolean useIntrinsicDimensions) {
        this.mDrawable = drawable;
        this.mExplanation = explanation;
        this.mUseIntrinsicDimensions = useIntrinsicDimensions;
    }

    public AssistantDrawable getDrawable() {
        return mDrawable;
    }

    public String getExplanation() {
        return mExplanation;
    }

    public boolean getUseIntrinsicDimensions() {
        return mUseIntrinsicDimensions;
    }

    /** Create infobox with the given values. */
    @CalledByNative
    private static AssistantInfoBox create(@Nullable AssistantDrawable drawable, String explanation,
            boolean useIntrinsicDimensions) {
        return new AssistantInfoBox(drawable, explanation, useIntrinsicDimensions);
    }
}
