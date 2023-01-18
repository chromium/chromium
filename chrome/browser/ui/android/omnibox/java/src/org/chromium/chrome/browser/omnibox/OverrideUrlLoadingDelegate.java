// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.ui.base.PageTransition;

/**
 * Delegate interface that allows implementers to override the default URL loading behavior of the
 * LocationBar.
 */
public interface OverrideUrlLoadingDelegate {
    /**
     * Returns true if the delegate will handle loading for the given parameters.
     */
    boolean willHandleLoadUrlWithPostData(String url, @PageTransition int transition,
            long inputStart, String postDataType, byte[] postData, boolean incognito);
}
