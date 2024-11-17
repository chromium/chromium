// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Default implementation of the delegate that allows implementers to override the default URL
 * loading behavior of the LocationBar.
 */
public class OverrideUrlLoadingDelegateImpl implements OverrideUrlLoadingDelegate {
    private @Nullable Runnable mOpenGridTabSwitcher;

    public void setOpenGridTabSwitcherCallback(Runnable callback) {
        mOpenGridTabSwitcher = callback;
    }

    /**
     * Evaluate whether supplied LoadUrlParams need special handling.
     *
     * @param params the parameters specifying what URL to load - and how
     * @param incognito whether URL is being opened from an incognito mode
     * @return true if the delegate will handle loading for the given parameters
     */
    @Override
    public boolean willHandleLoadUrlWithPostData(OmniboxLoadUrlParams params, boolean incognito) {
        if (TextUtils.equals(params.url, UrlConstants.GRID_TAB_SWITCHER_URL)) {
            if (mOpenGridTabSwitcher != null) {
                mOpenGridTabSwitcher.run();
                return true;
            }
        }
        return false;
    }
}
