// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.regional_capabilities;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link RegionalCapabilitiesService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RegionalCapabilitiesServiceUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock RegionalCapabilitiesService.Natives mMockServiceJni;

    @Before
    public void setUp() {
        RegionalCapabilitiesServiceJni.setInstanceForTesting(mMockServiceJni);
    }

    @Test
    public void isInEeaCountry() {
        when(mMockServiceJni.isInEeaCountry(anyLong())).thenReturn(true);
        RegionalCapabilitiesService service = new RegionalCapabilitiesService(42L);
        assertTrue(service.isInEeaCountry());
        verify(mMockServiceJni).isInEeaCountry(42L);
    }
}
