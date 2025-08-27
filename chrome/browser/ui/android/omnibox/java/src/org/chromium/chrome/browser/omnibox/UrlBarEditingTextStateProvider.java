// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;

/** Provider of editing text state from the UrlBar/Omnibox. */
@NullMarked
public interface UrlBarEditingTextStateProvider {
    /** Return the starting selection index for the text. */
    int getSelectionStart();

    /** Return the ending selection index for the text. */
    int getSelectionEnd();

    /** Return whether the view can accept autocomplete. */
    boolean shouldAutocomplete();

    /** Return whether the last edit was the result of a paste operation. */
    boolean wasLastEditPaste();

    /** Return the full text with any inline autocomplete. */
    String getTextWithAutocomplete();

    /** Return the text excluding any inline autocomplete. */
    String getTextWithoutAutocomplete();
}
