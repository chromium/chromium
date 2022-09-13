// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.form;

class AssistantFormSelectionChoice {
    private final String mLabel;
    private final String mDescriptionLine1;
    private final String mDescriptionLine2;
    private final boolean mIsInitiallySelected;

    public AssistantFormSelectionChoice(String label, String descriptionLine1,
            String descriptionLine2, boolean isInitiallySelected) {
        mLabel = label;
        mDescriptionLine1 = descriptionLine1;
        mDescriptionLine2 = descriptionLine2;
        mIsInitiallySelected = isInitiallySelected;
    }

    public String getLabel() {
        return mLabel;
    }

    public String getDescriptionLine1() {
        return mDescriptionLine1;
    }

    public String getDescriptionLine2() {
        return mDescriptionLine2;
    }

    public boolean isInitiallySelected() {
        return mIsInitiallySelected;
    }
}
