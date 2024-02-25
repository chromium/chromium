// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/** Interface for a page info main page controller. */
public interface PageInfoMainController {
    /**
     * Launches the PageInfoSubpage provided by |pageInfoCookiesController|.
     * @param controller The controller providing a PageInfoSubpage.
     */
    void launchSubpage(PageInfoSubpageController controller);

    /** Switches back to the main page info view. */
    void exitSubpage();

    /**
     * Record a user action.
     *
     * @param action The action to record.
     */
    void recordAction(@PageInfoAction int action);

    /** Refreshes the permissions of the page info. */
    void refreshPermissions();

    /** Returns a valid ConnectionSecurityLevel. */
    @ConnectionSecurityLevel
    int getSecurityLevel();

    /** @return A BrowserContext for this dialog. */
    BrowserContextHandle getBrowserContext();

    /** @return The Activity associated with the controller. */
    @Nullable
    Activity getActivity();

    /** @return The GURL of the page associated with the controller. */
    GURL getURL();

    /** Dismiss the page info dialog. */
    void dismiss();
}
