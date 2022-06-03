// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

/**
 * Provider of editing text state from the UrlBar/Omnibox.
 */
public interface UrlBarEditingTextStateProvider {
    /** Return the starting selection index for the text. */
    public int getSelectionStart();

    /** Return the ending selection index for the text. */
    public int getSelectionEnd();

    /** Return whether the view can accept autocomplete. */
    public boolean shouldAutocomplete();

    /** Return whether the last edit was the result of a paste operation. */
    public boolean wasLastEditPaste();

    /** Return the full text with any inline autocomplete. */
    public String getTextWithAutocomplete();

    /** Return the text excluding any inline autocomplete. */
    public String getTextWithoutAutocomplete();
}
