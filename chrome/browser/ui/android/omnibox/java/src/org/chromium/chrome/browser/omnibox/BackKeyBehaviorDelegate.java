// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

/**
 * Delegate interface that allows implementers to override the default back key behavior of the
 * LocationBar.
 */
public interface BackKeyBehaviorDelegate {
    /** Returns true if the delegate will handle the back key event. */
    default boolean handleBackKeyPressed() {
        return false;
    }
}
