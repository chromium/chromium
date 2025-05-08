// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import static org.chromium.ui.util.TokenHolder.INVALID_TOKEN;

import org.chromium.build.annotations.NullMarked;

/** Provides public API to manipulate web app header state like controls enabled state. */
@NullMarked
public interface WebAppHeaderDelegate {

    /**
     * Disables all controls in header and doesn't make them enabled until token is released.
     * Automatically clears releases previous token and gives a new one.
     *
     * @param token previous token that was acquired with this call, pass {@link INVALID_TOKEN} as
     *     initial token.
     * @return a new token that keeps disabled state.
     */
    int disableControlsAndClearOldToken(int token);

    /**
     * Enables controls by releasing token if it's the last one that prevents controls being
     * enabled.
     *
     * @param token that was acquired by calling {@link
     *     WebAppHeaderDelegate#disableControlsAndClearOldToken(int)}
     */
    void releaseDisabledControlsToken(int token);
}
