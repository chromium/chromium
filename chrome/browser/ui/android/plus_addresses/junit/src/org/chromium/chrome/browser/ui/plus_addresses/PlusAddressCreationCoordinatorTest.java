// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlusAddressCreationCoordinatorTest {
    private static final String MODAL_TITLE = "lorem ipsum title";
    private static final String MODAL_PLUS_ADDRESS_DESCRIPTION =
            "lorem ipsum description <link>test link</link> <b>test bold</b>";
    private static final String MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER = "plus+1@plus.plus";
    private static final String MODAL_OK = "ok";
    private static final String MODAL_CANCEL = "cancel";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private PlusAddressCreationViewBridge mBridge;
    @Mock private PlusAddressCreationMediator mMediator;

    private MockTabModel mTabModel;
    private PlusAddressCreationCoordinator mCoordinator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.setupActivity(TestActivity.class);
        mTabModel = new MockTabModel(mProfile, null);
        mCoordinator =
                new PlusAddressCreationCoordinator(
                        activity,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mBridge,
                        MODAL_TITLE,
                        MODAL_PLUS_ADDRESS_DESCRIPTION,
                        MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                        MODAL_OK,
                        MODAL_CANCEL);
        mCoordinator.setMediatorForTesting(mMediator);
    }

    @Test
    @SmallTest
    public void testRequestShowContent_callsMediatorRequestShow() {
        mCoordinator.requestShowContent();
        verify(mMediator).requestShowContent();
    }

    @Test
    @SmallTest
    public void testDestroy_callsMediatorDestroy() {
        mCoordinator.destroy();
        verify(mMediator).destroy();
    }
}
