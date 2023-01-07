// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.prefeditor;

/** The interface for editor fields that handle validation, display errors, and can be updated. */
public interface EditorFieldView {
    /**
     * Updates the error display.
     *
     * @param showError If true, displays the error message.  If false, clears it.
     */
    void updateDisplayedError(boolean showError);

    /** @return True if this field is valid. */
    boolean isValid();

    /** @return True if this field is required. */
    boolean isRequired();

    /** Scrolls to and focuses the field to bring user's attention to it. */
    void scrollToAndFocus();

    /** Rereads the field value from the model, which may have been updated. */
    void update();
}
