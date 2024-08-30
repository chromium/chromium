// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties.PLUS_PROFILE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.QUERY_HINT;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.TITLE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.WARNING;

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
    private static final String BOTTOMSHEET_TITLE = "Bottom sheet title";
    private static final String BOTTOMSHEET_WARNING = "Bottom sheet warning";
    private static final String BOTTOMSHEET_QUERY_HINT = "Query hint";
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
        AllPlusAddressesBottomSheetUIInfo info = new AllPlusAddressesBottomSheetUIInfo();
        info.setTitle(BOTTOMSHEET_TITLE);
        info.setWarning(BOTTOMSHEET_WARNING);
        info.setQueryHint(BOTTOMSHEET_QUERY_HINT);
        info.setPlusProfiles(List.of(PROFILE_1));

        mMediator.showPlusProfiles(info);

        assertEquals(mModel.get(TITLE), BOTTOMSHEET_TITLE);
        assertEquals(mModel.get(WARNING), BOTTOMSHEET_WARNING);
        assertEquals(mModel.get(QUERY_HINT), BOTTOMSHEET_QUERY_HINT);
        assertEquals(mModel.get(PLUS_PROFILES).size(), 1);
        assertEquals(mModel.get(PLUS_PROFILES).get(0).model.get(PLUS_PROFILE), PROFILE_1);
    }
}
