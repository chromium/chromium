// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/** Listener to be notified on url focus changes. */
@NullMarked
public interface UrlFocusChangeListener {
    /**
     * Triggered before the URL input field focus is requested.
     *
     * <p><b>Note:</b> This method is not guaranteed to be called in all scenarios where the URL
     * input field gains focus.
     *
     * @param tab The currently active {@link Tab}.
     */
    default void onUrlFocusWillBeRequested(@Nullable Tab tab) {}

    /**
     * Triggered when the URL input field has gained or lost focus.
     *
     * @param hasFocus Whether the URL field has gained focus.
     */
    void onUrlFocusChange(boolean hasFocus);

    /**
     * A notification that animations for focusing or unfocusing the input field has finished.
     *
     * @param hasFocus Whether the URL field has gained focus.
     */
    default void onUrlAnimationFinished(boolean hasFocus) {}
}
