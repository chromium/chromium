// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data.additional_sections;

import android.view.View;

import org.chromium.components.autofill_assistant.generic_ui.AssistantValue;

/** Interface for an additional section of the user data form. */
public interface AssistantAdditionalSection {
    /** Delegate interface for generic key/value widgets. */
    interface Delegate {
        void onValueChanged(String key, AssistantValue value);
        void onInputTextFocusChanged(boolean isFocused);
    }

    /** Returns the root view of the section. */
    View getView();

    /** Sets the padding for the top-most and the bottom-most view, respectively. */
    void setPaddings(int topPadding, int bottomPadding);

    /** Sets the delegate to notify for changes. */
    void setDelegate(Delegate delegate);
}
