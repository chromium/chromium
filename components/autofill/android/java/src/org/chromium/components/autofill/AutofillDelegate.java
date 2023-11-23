// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

/** An interface to handle the touch interaction with an autofill popup or keyboard accessory. */
public interface AutofillDelegate {
    /** Informs the controller the AutofillPopup or AutofillKeyboardAccessory was hidden. */
    public void dismissed();

    /**
     * Handles the selection of an Autofill suggestion from an AutofillPopup or
     * AutofillKeyboardAccessory.
     * @param listIndex The index of the selected Autofill suggestion.
     */
    public void suggestionSelected(int listIndex);

    /**
     * Initiates the deletion process for an item. (A confirm dialog should be shown.)
     * @param listIndex The index of the suggestion to delete.
     */
    public void deleteSuggestion(int listIndex);

    /**
     * Informs the controller the AutofillPopup received a
     * {@code TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED} accessibility event.
     */
    public void accessibilityFocusCleared();
}
