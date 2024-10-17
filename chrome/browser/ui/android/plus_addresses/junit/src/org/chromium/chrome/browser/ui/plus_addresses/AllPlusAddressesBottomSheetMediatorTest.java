// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties.PLUS_PROFILE;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AllPlusAddressesBottomSheetMediatorTest {
    private static final PlusProfile PROFILE_1 =
            new PlusProfile("example@gmail.com", "google.com", "https://google.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private AllPlusAddressesBottomSheetCoordinator.Delegate mDelegate;

    private PropertyModel mModel;
    private AllPlusAddressesBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel = AllPlusAddressesBottomSheetProperties.createDefaultModel();
        mMediator = new AllPlusAddressesBottomSheetMediator(mModel, mDelegate);
    }

    @Test
    @SmallTest
    public void testShowAndHideBottomSheet() {
        mMediator.showPlusProfiles(List.of(PROFILE_1));

        assertEquals(mModel.get(PLUS_PROFILES).size(), 1);
        assertEquals(mModel.get(PLUS_PROFILES).get(0).model.get(PLUS_PROFILE), PROFILE_1);
    }
}
