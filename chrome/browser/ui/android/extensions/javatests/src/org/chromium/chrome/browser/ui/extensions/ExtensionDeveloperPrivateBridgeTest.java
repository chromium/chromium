// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Tests for {@link ExtensionDeveloperPrivateBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionDeveloperPrivateBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Captor private ArgumentCaptor<Bundle> mBundleCaptor;

    @Before
    public void setUp() {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
    }

    @Test
    public void testShowSiteSettings() {
        String extensionId = "test_extension_id";
        ExtensionDeveloperPrivateBridge.showSiteSettings(extensionId);

        verify(mSettingsNavigation)
                .startSettings(
                        any(Context.class),
                        eq(SingleWebsiteSettings.class),
                        mBundleCaptor.capture());

        Bundle bundle = mBundleCaptor.getValue();
        String expectedUrl = UrlConstants.CHROME_EXTENSION_SCHEME + "://" + extensionId;
        WebsiteAddress actualAddress =
                (WebsiteAddress) bundle.getSerializable(SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
        assertEquals(expectedUrl, actualAddress.getOrigin());
    }
}
