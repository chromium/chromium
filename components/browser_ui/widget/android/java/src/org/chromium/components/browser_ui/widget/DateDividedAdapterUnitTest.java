// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;

import android.view.ViewGroup;

import androidx.annotation.Nullable;
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

import java.util.ArrayList;
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

    @Test
    public void testUpdateGroupItems_expandAndCollapse() {
        addHeaderGroup(/* addPersistentHeader= */ false);

        TestTimedItem head = new TestTimedItem(mTodayTimestamp, 1);
        TestTimedItem sub1 = new TestTimedItem(mTodayTimestamp, 2);
        TestTimedItem sub2 = new TestTimedItem(mTodayTimestamp, 3);
        TestTimedItem other = new TestTimedItem(mTodayTimestamp, 4);

        mAdapter.loadItems(List.of(head, other));

        // Initial state: Standard Header (0), Date Header (1), Head (2), Other (3)
        assertEquals(4, mAdapter.getItemCount());
        assertEquals(head, mAdapter.getItemAt(2).second);
        assertEquals(other, mAdapter.getItemAt(3).second);
        assertEquals(2, head.getPosition());
        assertEquals(3, other.getPosition());

        // Expand: insert sub1 and sub2 after head (position 2)
        TestTimedItem expandedHead = new TestTimedItem(mTodayTimestamp, 1);
        mAdapter.updateGroupItems(2, expandedHead, 0, List.of(sub1, sub2));

        // Expected state: Standard Header (0), Date Header (1), ExpandedHead (2), Sub1 (3), Sub2
        // (4), Other (5)
        assertEquals(6, mAdapter.getItemCount());
        assertEquals(expandedHead, mAdapter.getItemAt(2).second);
        assertEquals(sub1, mAdapter.getItemAt(3).second);
        assertEquals(sub2, mAdapter.getItemAt(4).second);
        assertEquals(other, mAdapter.getItemAt(5).second);
        assertEquals(2, expandedHead.getPosition());
        assertEquals(3, sub1.getPosition());
        assertEquals(4, sub2.getPosition());
        assertEquals(5, other.getPosition());

        // Collapse: remove 2 items after ExpandedHead (position 2)
        TestTimedItem collapsedHead = new TestTimedItem(mTodayTimestamp, 1);
        mAdapter.updateGroupItems(2, collapsedHead, 2, null);

        // Expected state: Standard Header (0), Date Header (1), CollapsedHead (2), Other (3)
        assertEquals(4, mAdapter.getItemCount());
        assertEquals(collapsedHead, mAdapter.getItemAt(2).second);
        assertEquals(other, mAdapter.getItemAt(3).second);
        assertEquals(2, collapsedHead.getPosition());
        assertEquals(3, other.getPosition());
    }

    @Test
    public void testUpdateGroupItems_passesPayload() {
        addHeaderGroup(/* addPersistentHeader= */ false);

        TestTimedItem head = new TestTimedItem(mTodayTimestamp, 1);
        mAdapter.loadItems(List.of(head));

        final ArrayList<Object> receivedPayloads = new ArrayList<>();
        mAdapter.registerAdapterDataObserver(
                new RecyclerView.AdapterDataObserver() {
                    @Override
                    public void onItemRangeChanged(
                            int positionStart, int itemCount, @Nullable Object payload) {
                        if (payload != null) {
                            receivedPayloads.add(payload);
                        }
                    }
                });

        TestTimedItem newHead = new TestTimedItem(mTodayTimestamp, 1);
        // Position of head is 2 (Standard Header at 0, Date Header at 1)
        mAdapter.updateGroupItems(2, newHead, 0, null);

        assertEquals(1, receivedPayloads.size());
        assertEquals(newHead, receivedPayloads.get(0));
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
