// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;

@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineChoiceServiceUnitTest {

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock Context mContext;
    private @Mock SearchEngineCountryDelegate mSearchEngineCountryDelegate;

    @Before
    public void setUp() {
        doReturn(Promise.rejected()).when(mSearchEngineCountryDelegate).getDeviceCountry();
    }

    @Test
    public void testAbstractDelegate() {
        var service =
                new SearchEngineChoiceService(
                        new SearchEngineCountryDelegate(mContext) {
                            @Override
                            public Promise<String> getDeviceCountry() {
                                return Promise.rejected();
                            }
                        });

        // The default implementation should be set to not trigger anything disruptive.
        assertFalse(service.isDeviceChoiceDialogEligible());
        assertTrue(service.shouldShowDeviceChoiceDialog().isFulfilled());
        assertFalse(service.shouldShowDeviceChoiceDialog().getResult());
    }
}
