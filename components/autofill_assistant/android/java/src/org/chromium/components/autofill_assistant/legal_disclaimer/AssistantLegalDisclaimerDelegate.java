// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.legal_disclaimer;

/**
 * Common interface for autofill assistant legal disclaimer delegates.
 */
public interface AssistantLegalDisclaimerDelegate {
    /**
     * Called when a text link of the legal disclaimer message <link0>text</link0> is clicked.
     */
    void onLinkClicked(int link);
}
