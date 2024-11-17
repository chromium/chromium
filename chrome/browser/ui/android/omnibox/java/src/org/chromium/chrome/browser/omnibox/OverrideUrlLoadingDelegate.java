// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;

/**
 * Delegate interface that allows implementers to override the default URL loading behavior of the
 * LocationBar.
 */
public interface OverrideUrlLoadingDelegate {
    /**
     * Evaluate whether supplied LoadUrlParams need special handling.
     *
     * @param params the parameters specifying what URL to load - and how
     * @param incognito whether URL is being opened from an incognito mode
     * @return true if the delegate will handle loading for the given parameters
     */
    boolean willHandleLoadUrlWithPostData(OmniboxLoadUrlParams params, boolean incognito);
}
