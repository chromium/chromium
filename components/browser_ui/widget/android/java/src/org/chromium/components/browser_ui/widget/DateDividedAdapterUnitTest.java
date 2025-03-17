// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;

import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.HeaderItemGroup;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.ItemGroup;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.ItemViewType;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.PersistentHeaderItem;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.StandardHeaderItem;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.TimedItem;

import java.util.Date;
import java.util.List;

/** Unit test for {@link DateDividedAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DateDividedAdapterUnitTest {
    private static class TestDateDividedAdapter extends DateDividedAdapter {
        @Override
        protected RecyclerView.ViewHolder createViewHolder(ViewGroup parent) {
            return null;
        }

        @Override
        protected void bindViewHolderForTimedItem(
                RecyclerView.ViewHolder viewHolder, DateDividedAdapter.TimedItem item) {}

        @Override
        protected int getTimedItemViewResId() {
            return 0;
        }
    }

    private static class TestTimedItem extends TimedItem {
        private final long mTimestamp;
        private final long mStableId;

        TestTimedItem(long timestamp, long stableId) {
            mTimestamp = timestamp;
            mStableId = stableId;
        }

        @Override
        public long getTimestamp() {
            return mTimestamp;
        }

        @Override
        public long getStableId() {
            return mStableId;
        }
    }

    private TestDateDividedAdapter mAdapter;
    private final long mTodayTimestamp = new Date().getTime();

    @Before
    public void setUp() {
        mAdapter = new TestDateDividedAdapter();
    }

    @Test
    public void testRemoveLastItem_withoutPersistentHeader() {
        addHeaderGroup(/* addPersistentHeader= */ false);

        TimedItem item = new TestTimedItem(mTodayTimestamp, 1);
        mAdapter.loadItems(List.of(item));

        // Verify that the list contains one standard header, a date header and an item.
        assertEquals(3, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.DATE, mAdapter.getItemViewType(1));
        assertEquals(item, mAdapter.getItemAt(2).second);

        mAdapter.removeItem(item);

        // Verify that the list is empty.
        assertEquals(0, mAdapter.getItemCount());
    }

    @Test
    public void testRemoveLastItem_withPersistentHeader() {
        addHeaderGroup(/* addPersistentHeader= */ true);

        TimedItem item = new TestTimedItem(mTodayTimestamp, 1);
        mAdapter.loadItems(List.of(item));

        // Verify that the list contains a standard header, a persistent header, a date header and
        // an item.
        assertEquals(4, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.PERSISTENT_HEADER, mAdapter.getItemViewType(1));
        assertEquals(ItemViewType.DATE, mAdapter.getItemViewType(2));
        assertEquals(item, mAdapter.getItemAt(3).second);

        mAdapter.removeItem(item);

        // Verify that only the two headers remain.
        assertEquals(2, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.PERSISTENT_HEADER, mAdapter.getItemViewType(1));
    }

    @Test
    public void testRemoveOneItem_withoutPersistentHeader() {
        addHeaderGroup(/* addPersistentHeader= */ false);

        TimedItem item1 = new TestTimedItem(mTodayTimestamp, 1);
        TimedItem item2 = new TestTimedItem(mTodayTimestamp, 2);
        mAdapter.loadItems(List.of(item1, item2));

        // Verify that the list contains a standard header, a date header and two items.
        assertEquals(4, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.DATE, mAdapter.getItemViewType(1));
        assertEquals(item1, mAdapter.getItemAt(2).second);
        assertEquals(item2, mAdapter.getItemAt(3).second);

        mAdapter.removeItem(item1);

        // Verify that the list contains the headers and the remaining item.
        assertEquals(3, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.DATE, mAdapter.getItemViewType(1));
        assertEquals(item2, mAdapter.getItemAt(2).second);
    }

    @Test
    public void testRemoveOneItem_withPersistentHeader() {
        addHeaderGroup(/* addPersistentHeader= */ true);

        TimedItem item1 = new TestTimedItem(mTodayTimestamp, 1);
        TimedItem item2 = new TestTimedItem(mTodayTimestamp, 2);
        mAdapter.loadItems(List.of(item1, item2));

        // Verify that the list contains a standard header, a persistent header, a date header and
        // two items.
        assertEquals(5, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.PERSISTENT_HEADER, mAdapter.getItemViewType(1));
        assertEquals(ItemViewType.DATE, mAdapter.getItemViewType(2));
        assertEquals(item1, mAdapter.getItemAt(3).second);
        assertEquals(item2, mAdapter.getItemAt(4).second);

        mAdapter.removeItem(item1);

        // Verify that the list contains the headers and the remaining item.
        assertEquals(4, mAdapter.getItemCount());
        assertEquals(ItemViewType.STANDARD_HEADER, mAdapter.getItemViewType(0));
        assertEquals(ItemViewType.PERSISTENT_HEADER, mAdapter.getItemViewType(1));
        assertEquals(ItemViewType.DATE, mAdapter.getItemViewType(2));
        assertEquals(item2, mAdapter.getItemAt(3).second);
    }

    private void addHeaderGroup(boolean addPersistentHeader) {
        ItemGroup headerGroup = new HeaderItemGroup();
        headerGroup.addItem(new StandardHeaderItem(0, null));
        if (addPersistentHeader) {
            headerGroup.addItem(new PersistentHeaderItem(1, null));
        }
        mAdapter.addGroup(headerGroup);
    }
}
