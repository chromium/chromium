// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

/**
 * The set of operations that inform the C++ side of actions taken.
 */
public interface PlusAddressCreationDelegate {
    public void onConfirmed();
    public void onCanceled();
    public void onPromptDismissed();
}
