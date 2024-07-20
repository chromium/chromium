// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View.MeasureSpec;
import android.widget.SearchView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.base.TestActivity;

import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class AllPlusAddressesBottomSheetModuleTest {
    private static final long TEST_NATIVE = 100;
    private static final int WIDTH = 2000;
    private static final int HEIGHT = 2000;
    private static final PlusProfile PROFILE_1 = new PlusProfile("google.com", "example@gmail.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Captor private ArgumentCaptor<AllPlusAddressesBottomSheetView> mViewCaptor;

    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private AllPlusAddressesBottomSheetCoordinator mCoordinator;
    private AllPlusAddressesBottomSheetUIInfo mUIInfo;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mCoordinator =
                new AllPlusAddressesBottomSheetCoordinator(mActivity, mBottomSheetController);
        mUIInfo = new AllPlusAddressesBottomSheetUIInfo();
        mUIInfo.setPlusProfiles(List.of(PROFILE_1));
    }

    @Test
    public void testShowPlusProfiles() {
        mCoordinator.showPlusProfiles(mUIInfo);
        verify(mBottomSheetController).requestShowContent(mViewCaptor.capture(), eq(true));

        AllPlusAddressesBottomSheetView view = mViewCaptor.getValue();
        assertNotNull(view);

        // Robolectric doesn't layout recycler views.
        layoutPlusAddressView(view);

        TextView origin = view.getContentView().findViewById(R.id.plus_profile_origin);
        assertNotNull(origin);
        assertEquals(origin.getText(), PROFILE_1.getOrigin());

        ChipView plusAddress = view.getContentView().findViewById(R.id.plus_address);
        assertNotNull(plusAddress);
        assertEquals(plusAddress.getPrimaryTextView().getText(), PROFILE_1.getPlusAddress());
    }

    @Test
    @SmallTest
    public void testFilterPlusProfiles() {
        mCoordinator.showPlusProfiles(mUIInfo);
        verify(mBottomSheetController).requestShowContent(mViewCaptor.capture(), eq(true));

        AllPlusAddressesBottomSheetView view = mViewCaptor.getValue();
        assertNotNull(view);

        // Robolectric doesn't layout recycler views.
        layoutPlusAddressView(view);

        RecyclerView profilesView = view.getContentView().findViewById(R.id.sheet_item_list);
        assertEquals(profilesView.getAdapter().getItemCount(), 1);

        SearchView searchView =
                view.getContentView().findViewById(R.id.all_plus_addresses_search_view);

        // Query by origin, the plus profile should stay.
        searchView.setQuery("google", /* submit= */ true);
        assertEquals(profilesView.getAdapter().getItemCount(), 1);

        // Query by email, the plus profile should stay.
        searchView.setQuery("gmail.com", /* submit= */ true);
        assertEquals(profilesView.getAdapter().getItemCount(), 1);

        // No profiles should be shown for this field.
        searchView.setQuery("fff", /* submit= */ true);
        assertEquals(profilesView.getAdapter().getItemCount(), 0);

        // All profiles should be displayed for an empty query.
        searchView.setQuery("", /* submit= */ true);
        assertEquals(profilesView.getAdapter().getItemCount(), 1);
    }

    /**
     * Triggers view holder creation by explicitly measuring and layouting the plus address views.
     */
    private void layoutPlusAddressView(AllPlusAddressesBottomSheetView view) {
        RecyclerView profilesView = view.getContentView().findViewById(R.id.sheet_item_list);
        profilesView.measure(
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        profilesView.layout(0, 0, WIDTH, HEIGHT);
    }
}
