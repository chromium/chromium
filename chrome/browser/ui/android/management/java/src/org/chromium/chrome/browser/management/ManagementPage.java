// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Native page that displays whether the current profile is managed or not. */
public class ManagementPage extends BasicNativePage {
    private final ManagementCoordinator mManagementCoordinator;
    private String mTitle;

    /**
     * Create a new instance of the management page.
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     */
    public ManagementPage(NativePageHost host, Profile profile) {
        super(host);

        mTitle = host.getContext().getResources().getString(R.string.management);
        mManagementCoordinator = new ManagementCoordinator(host, profile);

        initWithView(mManagementCoordinator.getView());
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.MANAGEMENT_HOST;
    }
}
