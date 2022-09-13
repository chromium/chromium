// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data.additional_sections;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

/** Interface for factories of additional user form sections. */
public interface AssistantAdditionalSectionFactory {
    /**
     * Instantiates the additional section for {@code context} and adds it at position {@code
     * index} to {@code parent}.
     */
    AssistantAdditionalSection createSection(Context context, ViewGroup parent, int index,
            @Nullable AssistantAdditionalSection.Delegate delegate);
}
