// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;

/** Tests for ProcessScopeDependencyProviderImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public final class ProcessScopeDependencyProviderImplTest {

    ProcessScopeDependencyProviderImpl mProvider;
    boolean mMetricsReportingEnabled;

    private PrivacyPreferencesManager mStubPrivacyPrefsManager =
            new PrivacyPreferencesManager() {
                @Override
                public boolean isMetricsReportingEnabled() {
                    return mMetricsReportingEnabled;
                }

                // Boilerplate.
                @Override
                public void setUsageAndCrashReporting(boolean enabled) {}

                @Override
                public void syncUsageAndCrashReportingPrefs() {}

                @Override
                public void setClientInSampleForMetrics(boolean inSample) {}

                @Override
                public boolean isClientInSampleForMetrics() {
                    return true;
                }

                @Override
                public void setClientInSampleForCrashes(boolean inSample) {}

                @Override
                public boolean isClientInSampleForCrashes() {
                    return true;
                }

                @Override
                public boolean isNetworkAvailableForCrashUploads() {
                    return true;
                }

                @Override
                public boolean isUsageAndCrashReportingPermittedByPolicy() {
                    return true;
                }

                @Override
                public boolean isUsageAndCrashReportingPermittedByUser() {
                    return true;
                }

                @Override
                public boolean isUploadEnabledForTests() {
                    return true;
                }

                @Override
                public boolean isMetricsUploadPermitted() {
                    return false;
                }

                @Override
                public void setMetricsReportingEnabled(boolean enabled) {}

                @Override
                public ObservableSupplier<Boolean>
                        getUsageAndCrashReportingPermittedObservableSupplier() {
                    return null;
                }
            };

    @Before
    public void setUp() {
        mProvider = new ProcessScopeDependencyProviderImpl("key", mStubPrivacyPrefsManager);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.XSURFACE_METRICS_REPORTING})
    public void usageAndCrashReporting_featureDisabled() {
        mMetricsReportingEnabled = false;
        assertFalse(mProvider.isXsurfaceUsageAndCrashReportingEnabled());

        mMetricsReportingEnabled = true;
        assertFalse(mProvider.isXsurfaceUsageAndCrashReportingEnabled());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.XSURFACE_METRICS_REPORTING})
    public void usageAndCrashReporting_featureEnabled() {
        mMetricsReportingEnabled = false;
        assertFalse(mProvider.isXsurfaceUsageAndCrashReportingEnabled());

        mMetricsReportingEnabled = true;
        assertTrue(mProvider.isXsurfaceUsageAndCrashReportingEnabled());
    }
}
