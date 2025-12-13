// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;
import java.util.List;

/** JUnit test for {@link RichRadioButtonList} logic. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class RichRadioButtonListUnitTest {

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private RichRadioButtonList mRichRadioButtonList;
    private RecyclerView mRecyclerView;

    @Mock private RichRadioButtonAdapter.OnItemSelectedListener mMockListener;
    @Captor private ArgumentCaptor<String> mStringCaptor;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View content =
                            LayoutInflater.from(sActivity)
                                    .inflate(
                                            R.layout.rich_radio_button_list_selection_test,
                                            null,
                                            false);
                    sActivity.setContentView(content);

                    mRichRadioButtonList = content.findViewById(R.id.test_rich_radio_button_list);
                    mRichRadioButtonList.setBackgroundColor(Color.WHITE);

                    mRecyclerView = mRichRadioButtonList.getRecyclerViewForTesting();
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertNotNull(
                "RichRadioButtonList should not be null after setup.", mRichRadioButtonList);
        Assert.assertNotNull("RecyclerView should not be null after setup.", mRecyclerView);
        Assert.assertNotNull("Mock listener should not be null after setup.", mMockListener);
    }

    private void performClickOnItem(int position) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView.ViewHolder viewHolder =
                            mRecyclerView.findViewHolderForAdapterPosition(position);
                    Assert.assertNotNull(
                            "ViewHolder at position " + position + " should not be null for click.",
                            viewHolder);
                    viewHolder.itemView.performClick();
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private @Nullable String getSelectedItemIdFromAdapter() {
        RichRadioButtonAdapter adapter = mRichRadioButtonList.getAdapterForTesting();
        Assert.assertNotNull(
                "Adapter should not be null when retrieving selected item ID.", adapter);
        return adapter.getSelectedItemIdForTesting();
    }

    private int getSelectedPositionFromAdapter() {
        RichRadioButtonAdapter adapter = mRichRadioButtonList.getAdapterForTesting();
        Assert.assertNotNull(
                "Adapter should not be null when retrieving selected position.", adapter);
        return adapter.getSelectedPositionForTesting();
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButtonList", "Selection"})
    public void testDefaultSelectionOnInitialization() throws Exception {
        List<RichRadioButtonData> options =
                Arrays.asList(
                        new RichRadioButtonData.Builder("first_item", "First").build(),
                        new RichRadioButtonData.Builder("second_item", "Second").build());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonList.initialize(
                            options,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            mMockListener);
                });

        verify(mMockListener, times(1)).onItemSelected(mStringCaptor.capture());
        Assert.assertEquals(
                "Listener should be notified that 'first_item' is selected by default.",
                "first_item",
                mStringCaptor.getValue());

        Assert.assertEquals(
                "Selected item ID from adapter should be 'first_item' after default selection.",
                "first_item",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position from adapter should be 0 after default selection.",
                0,
                getSelectedPositionFromAdapter());
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButtonList", "Selection"})
    public void testInitialSelectionVerticalSingleColumn() throws Exception {
        List<RichRadioButtonData> options =
                Arrays.asList(
                        new RichRadioButtonData.Builder("id_opt1", "Option 1").build(),
                        new RichRadioButtonData.Builder("id_opt2", "Option 2").build(),
                        new RichRadioButtonData.Builder("id_opt3", "Option 3").build());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonList.initialize(
                            options,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            mMockListener);
                    clearInvocations(mMockListener);
                    mRichRadioButtonList.setSelectedItem("id_opt2");
                });

        verify(mMockListener, times(1)).onItemSelected(mStringCaptor.capture());
        Assert.assertEquals(
                "Listener should be notified for 'id_opt2' selection on initial set.",
                "id_opt2",
                mStringCaptor.getValue());

        Assert.assertEquals(
                "Selected item ID from adapter should be 'id_opt2' after initial set.",
                "id_opt2",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position from adapter should be 1 after initial set.",
                1,
                getSelectedPositionFromAdapter());
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButtonList", "Selection"})
    public void testSelectionChangeVerticalSingleColumn() throws Exception {
        List<RichRadioButtonData> options =
                Arrays.asList(
                        new RichRadioButtonData.Builder("id_optA", "Option A").build(),
                        new RichRadioButtonData.Builder("id_optB", "Option B").build(),
                        new RichRadioButtonData.Builder("id_optC", "Option C").build());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonList.initialize(
                            options,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            mMockListener);
                    mRichRadioButtonList.setSelectedItem("id_optA");
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "Initial selected item ID should be 'id_optA'.",
                "id_optA",
                getSelectedItemIdFromAdapter());

        clearInvocations(mMockListener);

        performClickOnItem(1);
        verify(mMockListener, times(1)).onItemSelected(mStringCaptor.capture());
        Assert.assertEquals(
                "Listener should be notified for 'id_optB' selection after first click.",
                "id_optB",
                mStringCaptor.getValue());

        Assert.assertEquals(
                "Selected item ID should be 'id_optB' after first click.",
                "id_optB",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position should be 1 after first click.",
                1,
                getSelectedPositionFromAdapter());

        performClickOnItem(0);
        verify(mMockListener, times(2)).onItemSelected(mStringCaptor.capture());
        Assert.assertEquals(
                "Listener should be notified for 'id_optA' selection after second click.",
                "id_optA",
                mStringCaptor.getValue());
        Assert.assertEquals(
                "Selected item ID should be 'id_optA' after second click.",
                "id_optA",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position should be 0 after second click.",
                0,
                getSelectedPositionFromAdapter());
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButtonList", "Selection"})
    public void testSelectionWithinTwoColumnGrid() throws Exception {
        List<RichRadioButtonData> options =
                Arrays.asList(
                        new RichRadioButtonData.Builder("grid_top_left", "TL").build(),
                        new RichRadioButtonData.Builder("grid_top_right", "TR").build(),
                        new RichRadioButtonData.Builder("grid_bottom_left", "BL").build(),
                        new RichRadioButtonData.Builder("grid_bottom_right", "BR").build());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonList.initialize(
                            options, RichRadioButtonList.LayoutMode.TWO_COLUMN_GRID, mMockListener);
                    mRichRadioButtonList.setSelectedItem("grid_top_left");
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(
                "Initial selected item ID should be 'grid_top_left'.",
                "grid_top_left",
                getSelectedItemIdFromAdapter());

        clearInvocations(mMockListener);

        performClickOnItem(1);
        verify(mMockListener, times(1)).onItemSelected(mStringCaptor.capture());
        Assert.assertEquals(
                "Listener should be notified for 'grid_top_right' selection.",
                "grid_top_right",
                mStringCaptor.getValue());
        Assert.assertEquals(
                "Selected item ID should be 'grid_top_right' after first grid click.",
                "grid_top_right",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position should be 1 after first grid click.",
                1,
                getSelectedPositionFromAdapter());

        performClickOnItem(2);
        verify(mMockListener, times(2)).onItemSelected(mStringCaptor.capture());
        Assert.assertEquals(
                "Listener should be notified for 'grid_bottom_left' selection.",
                "grid_bottom_left",
                mStringCaptor.getValue());
        Assert.assertEquals(
                "Selected item ID should be 'grid_bottom_left' after second grid click.",
                "grid_bottom_left",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position should be 2 after second grid click.",
                2,
                getSelectedPositionFromAdapter());
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButtonList", "Selection"})
    public void testSelectingAlreadySelectedItem() throws Exception {
        List<RichRadioButtonData> options =
                Arrays.asList(
                        new RichRadioButtonData.Builder("item1", "Item One").build(),
                        new RichRadioButtonData.Builder("item2", "Item Two").build());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonList.initialize(
                            options,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            mMockListener);
                    clearInvocations(mMockListener);
                    mRichRadioButtonList.setSelectedItem("item1");
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Click on the already selected item.
        performClickOnItem(0);

        // Expect no new interaction with the listener as the selection didn't change.
        verifyNoInteractions(mMockListener);

        Assert.assertEquals(
                "Selected item ID should remain 'item1' when re-selecting.",
                "item1",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position should remain 0 when re-selecting.",
                0,
                getSelectedPositionFromAdapter());
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButtonList", "Selection"})
    public void testSetSelectedItemWithNonExistentId() throws Exception {
        List<RichRadioButtonData> options =
                Arrays.asList(new RichRadioButtonData.Builder("itemX", "Item X").build());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonList.initialize(
                            options,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            mMockListener);
                    clearInvocations(mMockListener);
                });

        RuntimeException caughtException =
                Assert.assertThrows(
                        RuntimeException.class,
                        () ->
                                ThreadUtils.runOnUiThreadBlocking(
                                        () -> {
                                            mRichRadioButtonList.setSelectedItem("non_existent_id");
                                        }));

        Throwable cause = caughtException.getCause();
        Assert.assertTrue(
                "AssertionError message should indicate non-existent ID.",
                cause.getMessage()
                        .contains(
                                "Attempted to select an item with ID non_existent_id that is not in"
                                        + " the options list."));

        Assert.assertEquals(
                "Selected item ID should still be 'itemX' after a failed selection attempt.",
                "itemX",
                getSelectedItemIdFromAdapter());
        Assert.assertEquals(
                "Selected position should still be 0 after a failed selection attempt.",
                0,
                getSelectedPositionFromAdapter());
        verifyNoInteractions(mMockListener);
    }
}
