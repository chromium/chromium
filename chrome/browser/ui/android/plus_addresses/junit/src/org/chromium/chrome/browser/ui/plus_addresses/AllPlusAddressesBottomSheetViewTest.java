// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;

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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ItemType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AllPlusAddressesBottomSheetViewTest {
    private static final int WIDTH = 2000;
    private static final int HEIGHT = 2000;
    private static final String BOTTOMSHEET_TITLE = "Bottom sheet title";
    private static final String BOTTOMSHEET_WARNING = "Bottom sheet warning";
    private static final String BOTTOMSHEET_QUERY_HINT = "Query hint";
    private static final PlusProfile PROFILE_1 =
            new PlusProfile("example@gmail.com", "google.com", "https://google.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FaviconHelper mFaviconHelper;
    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private AllPlusAddressesBottomSheetView mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mView = new AllPlusAddressesBottomSheetView(mActivity, mBottomSheetController);
    }

    @Test
    @SmallTest
    public void testShowAndHideBottomSheet() {
        when(mBottomSheetController.requestShowContent(eq(mView), eq(true))).thenReturn(true);

        mView.setVisible(true);
        verify(mBottomSheetController).requestShowContent(mView, true);

        mView.setVisible(false);
        verify(mBottomSheetController).hideContent(mView, true);
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        mView.setTitle(BOTTOMSHEET_TITLE);
        TextView title = mView.getContentView().findViewById(R.id.sheet_title);
        assertEquals(title.getText(), BOTTOMSHEET_TITLE);
    }

    @Test
    @SmallTest
    public void testSetWarning() {
        mView.setWarning(BOTTOMSHEET_WARNING);
        TextView title = mView.getContentView().findViewById(R.id.sheet_warning);
        assertEquals(title.getText(), BOTTOMSHEET_WARNING);
    }

    @Test
    @SmallTest
    public void testQueryHint() {
        mView.setQueryHint(BOTTOMSHEET_QUERY_HINT);
        SearchView search =
                mView.getContentView().findViewById(R.id.all_plus_addresses_search_view);
        assertEquals(search.getQueryHint(), BOTTOMSHEET_QUERY_HINT);
    }

    @Test
    @SmallTest
    public void testSetOnQueryChangedCallback() {
        Callback<String> callback = mock(Callback.class);

        mView.setOnQueryChangedCallback(callback);

        SearchView searchView =
                mView.getContentView().findViewById(R.id.all_plus_addresses_search_view);
        searchView.setQuery("Test query", /* submit= */ true);
        verify(callback).onResult("Test query");
    }

    @Test
    @SmallTest
    public void testSetSheetItemListAdapter() {
        Callback<String> callback = mock(Callback.class);
        PropertyModel model = AllPlusAddressesBottomSheetProperties.createDefaultModel();
        model.get(PLUS_PROFILES)
                .add(
                        new ListItem(
                                ItemType.PLUS_PROFILE,
                                AllPlusAddressesBottomSheetProperties.PlusProfileProperties
                                        .createPlusProfileModel(PROFILE_1, callback)));
        mView.setSheetItemListAdapter(
                AllPlusAddressesBottomSheetCoordinator.createSheetItemListAdapter(
                        model.get(PLUS_PROFILES), mFaviconHelper));

        // Robolectric runner doesn't layout recycler views.
        layoutPlusAddressView();

        TextView origin = mView.getContentView().findViewById(R.id.plus_profile_origin);
        assertNotNull(origin);
        assertEquals(origin.getText(), PROFILE_1.getDisplayName());

        ChipView plusAddress = mView.getContentView().findViewById(R.id.plus_address);
        assertNotNull(plusAddress);
        assertEquals(plusAddress.getPrimaryTextView().getText(), PROFILE_1.getPlusAddress());

        plusAddress.performClick();
        verify(callback).onResult(PROFILE_1.getPlusAddress());
    }

    /**
     * Triggers view holder creation by explicitly measuring and layouting the plus address views.
     */
    private void layoutPlusAddressView() {
        RecyclerView profilesView = mView.getContentView().findViewById(R.id.sheet_item_list);
        profilesView.measure(
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        profilesView.layout(0, 0, WIDTH, HEIGHT);
    }
}
