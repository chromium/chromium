// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The class responsible for setting up ManagementPage.
 */
class ManagementCoordinator {
    private final ManagementView mView;

    /**
     * Creates a ManagementCoordinator for the ManagementPage.
     * @param context Environment Context.
     * @param profile The current Profile.
     */
    public ManagementCoordinator(Context context, Profile profile) {
        PropertyModel model =
                new PropertyModel.Builder(ManagementProperties.ALL_KEYS)
                        .with(ManagementProperties.BROWSER_IS_MANAGED,
                                ManagedBrowserUtils.hasBrowserPoliciesApplied(profile))
                        .with(ManagementProperties.ACCOUNT_MANAGER_NAME,
                                ManagedBrowserUtils.getAccountManagerName(profile))
                        .build();

        mView = (ManagementView) LayoutInflater.from(context).inflate(
                R.layout.enterprise_management, null);

        PropertyModelChangeProcessor.create(model, mView, ManagementViewBinder::bind);
    }

    /** Returns the intended view for ManagementPage tab. */
    public View getView() {
        return (View) mView;
    }
}
