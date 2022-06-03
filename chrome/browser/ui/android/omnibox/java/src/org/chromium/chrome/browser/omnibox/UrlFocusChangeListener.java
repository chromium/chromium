// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

/**
 * Listener to be notified on url focus changes.
 */
public interface UrlFocusChangeListener {
    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    void onUrlFocusChange(boolean hasFocus);

    /**
     * A notification that animations for focusing or unfocusing the input field has finished.
     * @param hasFocus Whether the URL field has gained focus.
     */
    default void onUrlAnimationFinished(boolean hasFocus) {};
}
