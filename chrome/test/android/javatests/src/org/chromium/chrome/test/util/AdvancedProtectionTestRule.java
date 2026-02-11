// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.junit.rules.ExternalResource;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.components.safe_browsing.OsAdditionalSecurityProvider;
import org.chromium.components.safe_browsing.OsAdditionalSecurityUtil;

/**
 * Test rule which installs a test OsAdditionalSecurityProvider. The provider needs to be installed
 * prior to the Profile being created. The logic is a test rule so that it can be used in
 * combination with test rules which launch an activity such as {@link BlankCTATabInitialStateRule}.
 */
public class AdvancedProtectionTestRule extends ExternalResource {

    private static class TestProvider extends OsAdditionalSecurityProvider {
        private boolean mIsAdvancedProtectionRequestedByOs;

        private final ObserverList<OsAdditionalSecurityProvider.Observer> mObserverList =
                new ObserverList<>();

        public void setIsAdvancedProtectionRequestedByOs(boolean isAdvancedProtectionRequested) {
            mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionRequested;
            for (OsAdditionalSecurityProvider.Observer observer : mObserverList) {
                observer.onAdvancedProtectionOsSettingChanged();
            }
        }

        @Override
        public void addObserver(OsAdditionalSecurityProvider.Observer observer) {
            mObserverList.addObserver(observer);
        }

        @Override
        public void removeObserver(OsAdditionalSecurityProvider.Observer observer) {
            mObserverList.removeObserver(observer);
        }

        @Override
        public boolean isAdvancedProtectionRequestedByOs() {
            return mIsAdvancedProtectionRequestedByOs;
        }
    }

    private TestProvider mProvider;

    @Override
    protected void before() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProvider = new TestProvider();
                    OsAdditionalSecurityUtil.setInstanceForTesting(mProvider);
                });
    }

    public void setIsAdvancedProtectionRequestedByOs(boolean isAdvancedProtectionRequestedByOs) {
        if (mProvider == null) return;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProvider.setIsAdvancedProtectionRequestedByOs(
                            isAdvancedProtectionRequestedByOs);
                });
    }
}
