// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

/** AutocompleteInput encompasses the input to autocomplete. */
public class AutocompleteInput {
    private int mPageClassification;

    /**
     * @param pageClassification The PageClassification for the input.
     */
    public AutocompleteInput(int pageClassification) {
        mPageClassification = pageClassification;
    }

    /** Returns the current page classification. */
    public int getPageClassification() {
        return mPageClassification;
    }
}
