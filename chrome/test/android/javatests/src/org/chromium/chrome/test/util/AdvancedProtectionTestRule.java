// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.content.Context;

import org.junit.rules.ExternalResource;

import org.chromium.base.ObserverList;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;

/**
 * Test rule which installs a test OsAdditionalSecurityPermissionProvider. The provider needs to be
 * installed prior to the Profile being created. The logic is a test rule so that it can be used in
 * combination with test rules which launch an activity such as {@link BlankCTATabInitialStateRule}.
 */
public class AdvancedProtectionTestRule extends ExternalResource {
    public static String TEST_JAVASCRIPT_OPTIMIZER_MESSAGE = "testJavascriptOptimizerMessage";

    private static class TestProvider extends OsAdditionalSecurityPermissionProvider {
        private boolean mIsAdvancedProtectionRequestedByOs;

        private final ObserverList<OsAdditionalSecurityPermissionProvider.Observer> mObserverList =
                new ObserverList<>();

        public void setIsAdvancedProtectionRequestedByOs(boolean isAdvancedProtectionRequested) {
            mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionRequested;
            for (OsAdditionalSecurityPermissionProvider.Observer observer : mObserverList) {
                observer.onAdvancedProtectionOsSettingChanged();
            }
        }

        @Override
        public void addObserver(OsAdditionalSecurityPermissionProvider.Observer observer) {
            mObserverList.addObserver(observer);
        }

        @Override
        public void removeObserver(OsAdditionalSecurityPermissionProvider.Observer observer) {
            mObserverList.removeObserver(observer);
        }

        @Override
        public boolean isAdvancedProtectionRequestedByOs() {
            return mIsAdvancedProtectionRequestedByOs;
        }

        @Override
        public String getJavascriptOptimizerMessage(Context context) {
            return AdvancedProtectionTestRule.TEST_JAVASCRIPT_OPTIMIZER_MESSAGE;
        }
    }

    private TestProvider mProvider;

    @Override
    protected void before() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProvider = new TestProvider();
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class, mProvider);
                });
    }

    @Override
    protected void after() {
        setIsAdvancedProtectionRequestedByOs(/* isAdvancedProtectionRequestedByOs= */ false);
        OsAdditionalSecurityPermissionUtil.resetForTesting();
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
