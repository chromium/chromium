// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The class responsible for setting up ManagementPage. */
class ManagementCoordinator {
    private final ManagementMediator mMediator;
    private final ManagementView mView;

    /**
     * Creates a ManagementCoordinator for the ManagementPage.
     * @param context Environment Context.
     * @param profile The current Profile.
     */
    public ManagementCoordinator(NativePageHost host, Profile profile) {
        mMediator = new ManagementMediator(host, profile);
        mView =
                (ManagementView)
                        LayoutInflater.from(host.getContext())
                                .inflate(R.layout.enterprise_management, null);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, ManagementViewBinder::bind);
    }

    /** Returns the intended view for ManagementPage tab. */
    public View getView() {
        return (View) mView;
    }
}
