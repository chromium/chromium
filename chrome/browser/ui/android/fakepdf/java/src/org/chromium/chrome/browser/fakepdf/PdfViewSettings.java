// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fakepdf;

/** Fake PDF viewer API. Will be removed once real APIs become available. */
public class PdfViewSettings {
    private final boolean mOverrideDefaultUrlClickBehavior;

    public PdfViewSettings(boolean overrideDefaultUrlClickBehavior) {
        this.mOverrideDefaultUrlClickBehavior = overrideDefaultUrlClickBehavior;
    }

    public boolean getOverrideDefaultUrlClickBehavior() {
        return this.mOverrideDefaultUrlClickBehavior;
    }
}
