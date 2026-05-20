// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.url.GURL;

import java.util.Objects;

/** Holds the runtime context for a single sign-in flow. */
@NullMarked
public class SigninDelegateContext extends DelegateContext {
    private static final String KEY_TAB_ID = "SigninDelegateContext.TabId";
    private static final String KEY_CONTINUE_URL = "SigninDelegateContext.ContinueUrl";

    private final @TabId int mTabId;
    private final GURL mContinueUrl;

    public SigninDelegateContext(@TabId int tabId, GURL continueUrl) {
        mTabId = tabId;
        mContinueUrl = continueUrl;
    }

    @Override
    public Bundle toBundle() {
        Bundle bundle = new Bundle();
        bundle.putInt(KEY_TAB_ID, mTabId);
        bundle.putString(KEY_CONTINUE_URL, mContinueUrl.getSpec());
        return bundle;
    }

    public static SigninDelegateContext fromBundle(Bundle bundle) {
        return new SigninDelegateContext(
                bundle.getInt(KEY_TAB_ID, Tab.INVALID_TAB_ID),
                new GURL(bundle.getString(KEY_CONTINUE_URL, "")));
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof SigninDelegateContext)) {
            return false;
        }
        SigninDelegateContext other = (SigninDelegateContext) object;
        return mTabId == other.mTabId && Objects.equals(mContinueUrl, other.mContinueUrl);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTabId, mContinueUrl);
    }

    public @TabId int getTabId() {
        return mTabId;
    }

    public GURL getContinueUrl() {
        return mContinueUrl;
    }
}
