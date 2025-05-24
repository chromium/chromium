// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class ThemeModuleUtilsUnitTest {
    private static final int TEST_THEME_OVERLAY_ID = 1000;

    @Test
    public void useEmptyProvider() {
        assertEquals(
                "Defaults to empty provider",
                0,
                ThemeModuleUtils.getProviderInstance().getThemeOverlay());
    }

    @Test
    public void useServiceLoader() {
        ThemeModuleUtils.setProviderInstanceForTesting(null);
        ServiceLoaderUtil.setInstanceForTesting(
                ThemeOverlayProvider.class, new TestThemeOverlayProvider());

        assertEquals(
                "Test provider should be in use.",
                TEST_THEME_OVERLAY_ID,
                ThemeModuleUtils.getProviderInstance().getThemeOverlay());
    }

    private static class TestThemeOverlayProvider implements ThemeOverlayProvider {

        @Override
        public int getThemeOverlay() {
            return TEST_THEME_OVERLAY_ID;
        }
    }
}
