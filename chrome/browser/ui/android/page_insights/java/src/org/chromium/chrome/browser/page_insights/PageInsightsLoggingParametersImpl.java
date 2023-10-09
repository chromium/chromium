// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.page_insights.proto.PageInsights.PageInsightsMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsLoggingParameters;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/** Implementation of {@link PageInsightsLoggingParameters}. */
class PageInsightsLoggingParametersImpl implements PageInsightsLoggingParameters {
    private final String mAccountName;
    private final byte[] mParentData;

    static PageInsightsLoggingParametersImpl create(
            Profile profile, PageInsightsMetadata metadata) {
        return new PageInsightsLoggingParametersImpl(
                getEmail(profile),
                metadata.hasLoggingCgi() ? metadata.getLoggingCgi().toByteArray() : null);
    }

    private PageInsightsLoggingParametersImpl(String accountName, byte[] parentData) {
        mAccountName = accountName;
        mParentData = parentData;
    }

    @Override
    public String accountName() {
        return mAccountName;
    }

    @Override
    public byte[] parentData() {
        return mParentData;
    }

    @Nullable
    private static String getEmail(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) {
            return null;
        }
        CoreAccountInfo accountInfo = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (accountInfo == null) {
            return null;
        }
        return accountInfo.getEmail();
    }
}
