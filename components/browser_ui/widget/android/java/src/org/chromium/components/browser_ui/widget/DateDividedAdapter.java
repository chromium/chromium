// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.components.browser_ui.util.date.CalendarFactory;
import org.chromium.components.browser_ui.util.date.StringUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * An {@link Adapter} that works with a {@link RecyclerView}. It sorts the given {@link List} of
 * {@link TimedItem}s according to their date, and divides them into sub lists and displays them in
 * different sections.
 * <p>
 * Subclasses should not care about the how date headers are placed in the list. Instead, they
 * should call {@link #loadItems(List)} with a list of {@link TimedItem}, and this adapter will
 * insert the headers automatically.
 */
public abstract class DateDividedAdapter extends Adapter<RecyclerView.ViewHolder> {
    static {
        CalendarFactory.warmUp();
    }

    /** Interface that the {@link Adapter} uses to interact with the items it manages. */
    public abstract static class TimedItem {
        /** Value indicating that a TimedItem is not currently being displayed. */
        public static final int INVALID_POSITION = -1;

        /** Position of the TimedItem in the list, or {@link #INVALID_POSITION} if not shown. */
        private int mPosition = INVALID_POSITION;

        private boolean mIsFirstInGroup;
        private boolean mIsLastInGroup;
        private boolean mIsDateHeader;

        /** See {@link #mPosition}. */
        private final void setPosition(int position) {
            mPosition = position;
        }

        /** See {@link #mPosition}. */
        public final int getPosition() {
            return mPosition;
        }

        /** @param isFirst Whether this item is the first in its group. */
        public final void setIsFirstInGroup(boolean isFirst) {
            mIsFirstInGroup = isFirst;
        }

        /** @param isLast Whether this item is the last in its group. */
        public final void setIsLastInGroup(boolean isLast) {
            mIsLastInGroup = isLast;
        }

        /** @return Whether this item is the first in its group. */
        public boolean isFirstInGroup() {
            return mIsFirstInGroup;
        }

        /** @return Whether this item is the last in its group. */
        public boolean isLastInGroup() {
            return mIsLastInGroup;
        }

        /** @return The timestamp for this item. */
        public abstract long getTimestamp();

        /**
         * Returns an ID that uniquely identifies this TimedItem and doesn't change.
         * To avoid colliding with IDs generated for Date headers, at least one of the upper 32
         * bits of the long should be set.
         * @return ID that can uniquely identify the TimedItem.
         */
        public abstract long getStableId();
    }

    /** Contains information of a single header that this adapter uses to manage headers. */
    public static class HeaderItem extends TimedItem {
        private final long mStableId;
        private final View mView;

        /**
         * Initialize stable id and view associated with this HeaderItem.
         * @param position Position of this HeaderItem in the header group.
         * @param view View associated with this HeaderItem.
         */
        public HeaderItem(int position, View view) {
            mStableId = getTimestamp() - position;
            mView = view;
        }

        @Override
        public long getTimestamp() {
            return Long.MAX_VALUE;
        }

        @Override
        public long getStableId() {
            return mStableId;
        }

        /** @return The View associated with this HeaderItem. */
        public View getView() {
            return mView;
        }
    }

    /**
     * Contains information of a single footer that this adapter uses to manage footers.
     * Share most of the same funcionality as a Header class.
     */
    public static class FooterItem extends HeaderItem {
        public FooterItem(int position, View view) {
            super(position, view);
        }

        @Override
        public long getTimestamp() {
            return Long.MIN_VALUE;
        }
    }

    /** An item representing a date header. */
    static class DateHeaderTimedItem extends TimedItem {
        private long mTimestamp;

        public DateHeaderTimedItem(long timestamp) {
            mTimestamp = getDateAtMidnight(timestamp).getTime();
        }

        @Override
        public long getTimestamp() {
            return mTimestamp;
        }

        @Override
        public long getStableId() {
            return getStableIdFromDate(new Date(getTimestamp()));
        }
    }

    /** A {@link RecyclerView.ViewHolder} that displays a date header. */
    public static class DateViewHolder extends RecyclerView.ViewHolder {
        private TextView mTextView;

        public DateViewHolder(View view) {
            super(view);
            if (view instanceof TextView) mTextView = (TextView) view;
        }

        /** @param date The date that this DateViewHolder should display. */
        public void setDate(Date date) {
            mTextView.setText(StringUtils.dateToHeaderString(date));
        }
    }

    protected static class BasicViewHolder extends RecyclerView.ViewHolder {
        public BasicViewHolder(View itemView) {
            super(itemView);
        }
    }

    protected static class SubsectionHeaderViewHolder extends RecyclerView.ViewHolder {
        private View mView;

        public SubsectionHeaderViewHolder(View itemView) {
            super(itemView);
            mView = itemView;
        }

        public View getView() {
            return mView;
        }
    }

    /**
     * A bucket of items with the same date. The date header should also be an item of the group.
     * Special groups are subclassed for list header(s) and list footers.
     */
    public static class ItemGroup {
        private final Date mDate;
        private final List<TimedItem> mItems = new ArrayList<>();

        /** Index of the header, relative to the full list.  Must be set only once.*/
        private int mIndex;

        private boolean mIsSorted;

        /** Constructors for groups that contain same date items. */
        public ItemGroup(long timestamp) {
            mDate = new Date(timestamp);
            mIsSorted = true;
        }

        /**
         * Default constructor for groups that don't contain same date items e.g. header, footer,
         * elevated priority groups etc.
         */
        public ItemGroup() {
            mDate = new Date(0L);
        }

        public void addItem(TimedItem item) {
            mItems.add(item);
            mIsSorted = mItems.size() == 1;
        }

        public void removeItem(TimedItem item) {
            mItems.remove(item);
        }

        public void removeAllItems() {
            mItems.clear();
        }

        /** Records the position of all the TimedItems in this group, relative to the full list. */
        public void setPosition(int index) {
            assert mIndex == 0 || mIndex == TimedItem.INVALID_POSITION;
            mIndex = index;

            sortIfNeeded();
            for (int i = 0; i < mItems.size(); i++) {
                TimedItem item = mItems.get(i);
                item.setPosition(index);
                item.setIsFirstInGroup(i == 0);
                item.setIsLastInGroup(i == mItems.size() - 1);
                index += 1;
            }
        }

        /** Unsets the position of all TimedItems in this group. */
        public void resetPosition() {
            mIndex = TimedItem.INVALID_POSITION;
            for (TimedItem item : mItems) item.setPosition(TimedItem.INVALID_POSITION);
        }

        /** @return Whether the given date happens in the same day as the items in this group. */
        public boolean isSameDay(Date otherDate) {
            return compareDate(mDate, otherDate) == 0;
        }

        /** @return The size of this group. */
        public int size() {
            return mItems.size();
        }

        /**
         * Used for sorting list groups.
         * @return The priority used to determine the position of this {@link ItemGroup} relative to
         * the top of the list.
         */
        @GroupPriority
        public int priority() {
            return GroupPriority.NORMAL_CONTENT;
        }

        /**
         * Returns the item to be displayed at the given index of this group.
         * @param index The index of the item.
         * @return The corresponding item.
         */
        public TimedItem getItemAt(int index) {
            assert index < size();
            sortIfNeeded();
            return mItems.get(index);
        }

        /** @return The view type associated for the given index */
        public @ItemViewType int getItemViewType(int index) {
            return mItems.get(index).mIsDateHeader ? ItemViewType.DATE : ItemViewType.NORMAL;
        }

        /**
         * Rather than sorting the list each time a new item is added, the list is sorted when
         * something requires a correct ordering of the items.
         */
        protected void sortIfNeeded() {
            if (mIsSorted) return;
            mIsSorted = true;

            Collections.sort(
                    mItems,
                    new Comparator<TimedItem>() {
                        @Override
                        public int compare(TimedItem lhs, TimedItem rhs) {
                            return compareItem(lhs, rhs);
                        }
                    });
        }

        /** Sorting function that determines the ordering of the items in this group. */
        protected int compareItem(TimedItem lhs, TimedItem rhs) {
            if (lhs.mIsDateHeader) return -1;
            if (rhs.mIsDateHeader) return 1;

            // More recent items are listed first.  Ideally we'd use Long.compare, but that
            // is an API level 19 call for some inexplicable reason.
            long timeDelta = lhs.getTimestamp() - rhs.getTimestamp();
            if (timeDelta > 0) {
                return -1;
            } else if (timeDelta == 0) {
                return 0;
            } else {
                return 1;
            }
        }
    }

    /** An item group representing the list header(s). */
    public static class HeaderItemGroup extends ItemGroup {
        @Override
        public @GroupPriority int priority() {
            return GroupPriority.HEADER;
        }

        @Override
        public @ItemViewType int getItemViewType(int index) {
            return ItemViewType.HEADER;
        }
    }

    /** An item group representing the list footer(s). */
    public static class FooterItemGroup extends ItemGroup {
        @Override
        public @GroupPriority int priority() {
            return GroupPriority.FOOTER;
        }

        @Override
        public @ItemViewType int getItemViewType(int index) {
            return ItemViewType.FOOTER;
        }
    }

    /** Specifies various view types of the list items for the purpose of recycling. */
    @IntDef({
        ItemViewType.FOOTER,
        ItemViewType.HEADER,
        ItemViewType.DATE,
        ItemViewType.NORMAL,
        ItemViewType.SUBSECTION_HEADER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemViewType {
        int FOOTER = -2;
        int HEADER = -1;
        int DATE = 0;
        int NORMAL = 1;
        int SUBSECTION_HEADER = 2;
    }

    /**
     * The priorities that determine the relative position of item groups starting at the top.
     * Default priority is GroupPriority.NORMAL_CONTENT.
     */
    @IntDef({
        GroupPriority.HEADER,
        GroupPriority.ELEVATED_CONTENT,
        GroupPriority.NORMAL_CONTENT,
        GroupPriority.FOOTER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface GroupPriority {
        int HEADER = 1;
        int ELEVATED_CONTENT = 2;
        int NORMAL_CONTENT = 3;
        int FOOTER = 4;
    }

    private static final String TAG = "DateDividedAdapter";

    private int mSize;

    private SortedSet<ItemGroup> mGroups =
            new TreeSet<>(
                    new Comparator<ItemGroup>() {
                        @Override
                        public int compare(ItemGroup lhs, ItemGroup rhs) {
                            if (lhs == rhs) return 0;

                            if (lhs.priority() != rhs.priority()) {
                                return lhs.priority() < rhs.priority() ? -1 : 1;
                            }

                            return compareDate(lhs.mDate, rhs.mDate);
                        }
                    });

    /**
     * Creates a {@link ViewHolder} in the given view parent.
     * @see #onCreateViewHolder(ViewGroup, int)
     */
    protected abstract ViewHolder createViewHolder(ViewGroup parent);

    /**
     * Creates a {@link BasicViewHolder} in the given view parent for the header. The default
     * implementation will create an empty FrameLayout container as the view holder.
     * @see #onCreateViewHolder(ViewGroup, int)
     */
    protected BasicViewHolder createHeader(ViewGroup parent) {
        // Create an empty layout as a container for the header view.
        View v =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.date_divided_adapter_header_view_holder, parent, false);
        return new BasicViewHolder(v);
    }

    /**
     * Creates a {@link BasicViewHolder} in the given view parent for the footer.
     * See {@link #onCreateViewHolder(ViewGroup, int)}.
     */
    @Nullable
    protected BasicViewHolder createFooter(ViewGroup parent) {
        return null;
    }

    /**
     * Creates a {@link DateViewHolder} in the given view parent.
     * @see #onCreateViewHolder(ViewGroup, int)
     */
    protected DateViewHolder createDateViewHolder(ViewGroup parent) {
        return new DateViewHolder(
                LayoutInflater.from(parent.getContext())
                        .inflate(getTimedItemViewResId(), parent, false));
    }

    /**
     * Creates a {@link ViewHolder} for a subsection in the given view parent.
     * @see #onCreateViewHolder(ViewGroup, int)
     */
    @Nullable
    protected SubsectionHeaderViewHolder createSubsectionHeader(ViewGroup parent) {
        return null;
    }

    /**
     * Helper function to determine whether an item is a subsection header.
     * @param timedItem The item.
     * @return Whether the item is a subsection header.
     */
    protected boolean isSubsectionHeader(TimedItem timedItem) {
        return false;
    }

    /**
     * Binds the {@link ViewHolder} with the given {@link TimedItem}.
     * @see #onBindViewHolder(ViewHolder, int)
     */
    protected abstract void bindViewHolderForTimedItem(ViewHolder viewHolder, TimedItem item);

    /**
     * Binds the {@link SubsectionHeaderViewHolder} with the given {@link TimedItem}.
     * @see #onBindViewHolder(ViewHolder, int)
     */
    protected void bindViewHolderForSubsectionHeader(
            SubsectionHeaderViewHolder holder, TimedItem timedItem) {}

    /**
     * Binds the {@link BasicViewHolder} with the given {@link HeaderItem}.
     * @see #onBindViewHolder(ViewHolder, int)
     */
    protected void bindViewHolderForHeaderItem(ViewHolder viewHolder, HeaderItem headerItem) {
        BasicViewHolder basicViewHolder = (BasicViewHolder) viewHolder;
        View v = headerItem.getView();
        ((ViewGroup) basicViewHolder.itemView).removeAllViews();
        if (v.getParent() != null) ((ViewGroup) v.getParent()).removeView(v);
        ((ViewGroup) basicViewHolder.itemView).addView(v);
    }

    /**
     * Binds the {@link BasicViewHolder} with the given {@link FooterItem}.
     * @see #onBindViewHolder(ViewHolder, int)
     */
    protected void bindViewHolderForFooterItem(ViewHolder viewHolder, FooterItem footerItem) {
        BasicViewHolder basicViewHolder = (BasicViewHolder) viewHolder;
        View v = footerItem.getView();
        ((ViewGroup) basicViewHolder.itemView).removeAllViews();
        if (v.getParent() != null) ((ViewGroup) v.getParent()).removeView(v);
        ((ViewGroup) basicViewHolder.itemView).addView(v);
    }

    /**
     * Gets the resource id of the view showing the date header.
     * Contract for subclasses: this view should be a {@link TextView}.
     */
    protected abstract int getTimedItemViewResId();

    /**
     * Loads a list of {@link TimedItem}s to this adapter. Previous data will not be removed. Call
     * {@link #clear(boolean)} to remove previous items.
     */
    public void loadItems(List<? extends TimedItem> timedItems) {
        for (TimedItem timedItem : timedItems) {
            Date date = new Date(timedItem.getTimestamp());
            boolean found = false;
            for (ItemGroup group : mGroups) {
                if (group.isSameDay(date)) {
                    found = true;
                    group.addItem(timedItem);
                    break;
                }
            }
            if (!found) {
                // Create a new ItemGroup with the date for the new item. Insert the date header and
                // the new item into the group.
                TimedItem dateHeader = new DateHeaderTimedItem(timedItem.getTimestamp());
                dateHeader.mIsDateHeader = true;
                ItemGroup newGroup = new ItemGroup(timedItem.getTimestamp());
                newGroup.addItem(dateHeader);
                newGroup.addItem(timedItem);
                mGroups.add(newGroup);
            }
        }

        setSizeAndGroupPositions();
        notifyDataSetChanged();
    }

    /** Tells each group where they start in the list. Also calculates the list size. */
    private void setSizeAndGroupPositions() {
        mSize = 0;
        for (ItemGroup group : mGroups) {
            group.resetPosition();
            group.setPosition(mSize);
            mSize += group.size();
        }
    }

    /**
     * The utility function to add an {@link ItemGroup}.
     * @param group The group to be added.
     */
    protected void addGroup(ItemGroup group) {
        mGroups.add(group);

        setSizeAndGroupPositions();
        notifyDataSetChanged();
    }

    /**
     * Add a list of headers as the first group in this adapter. If headerItems has no items,
     * the header group will not be created. Otherwise, header items will be added as child items
     * to the header group. Note that any previously added header items will be removed.
     * {@link #bindViewHolderForHeaderItem(ViewHolder, HeaderItem)} will bind the HeaderItem views
     * to the given ViewHolder. Sub-classes may override #bindViewHolderForHeaderItem and
     * (@link #createHeader(ViewGroup)} if custom behavior is needed.
     *
     * @param headerItems Zero or more header items to be add to the header item group.
     */
    public void setHeaders(HeaderItem... headerItems) {
        if (headerItems == null || headerItems.length == 0) {
            removeHeader();
            return;
        }

        if (hasListHeader()) mGroups.remove(mGroups.first());

        ItemGroup header = new HeaderItemGroup();
        for (HeaderItem item : headerItems) {
            header.addItem(item);
        }

        addGroup(header);
    }

    /** Removes the list header. */
    public void removeHeader() {
        if (!hasListHeader()) return;
        mGroups.remove(mGroups.first());

        setSizeAndGroupPositions();
        notifyDataSetChanged();
    }

    /** Whether the adapter has a list header. */
    public boolean hasListHeader() {
        return !mGroups.isEmpty() && mGroups.first().priority() == GroupPriority.HEADER;
    }

    /** Whether the adapter has a list header. */
    public boolean hasListFooter() {
        return !mGroups.isEmpty() && mGroups.last().priority() == GroupPriority.FOOTER;
    }

    /** Adds a footer as the last group in this adapter. */
    public void addFooter() {
        if (hasListFooter()) return;

        ItemGroup footer = new FooterItemGroup();
        addGroup(footer);
    }

    /** Removes the footer group if present. */
    public void removeFooter() {
        if (!hasListFooter()) return;

        mGroups.remove(mGroups.last());
        setSizeAndGroupPositions();
        notifyDataSetChanged();
    }

    /**
     * Removes all items from this adapter.
     * @param notifyDataSetChanged Whether to notify that the data set has been changed.
     */
    public void clear(boolean notifyDataSetChanged) {
        mSize = 0;

        // Unset the positions of all items in the list.
        for (ItemGroup group : mGroups) group.resetPosition();
        mGroups.clear();

        if (notifyDataSetChanged) notifyDataSetChanged();
    }

    @Override
    public long getItemId(int position) {
        if (!hasStableIds()) return RecyclerView.NO_ID;

        Pair<Date, TimedItem> pair = getItemAt(position);
        return pair.second == null ? getStableIdFromDate(pair.first) : pair.second.getStableId();
    }

    /** Gets the item at the given position. */
    public Pair<Date, TimedItem> getItemAt(int position) {
        Pair<ItemGroup, Integer> pair = getGroupAt(position);
        ItemGroup group = pair.first;
        return new Pair<>(group.mDate, group.getItemAt(pair.second));
    }

    @Override
    @ItemViewType
    public final int getItemViewType(int position) {
        Pair<ItemGroup, Integer> pair = getGroupAt(position);
        ItemGroup group = pair.first;
        return group.getItemViewType(pair.second);
    }

    @Override
    public final RecyclerView.ViewHolder onCreateViewHolder(
            ViewGroup parent, @ItemViewType int viewType) {
        switch (viewType) {
            case ItemViewType.DATE:
                return createDateViewHolder(parent);
            case ItemViewType.NORMAL:
                return createViewHolder(parent);
            case ItemViewType.HEADER:
                return createHeader(parent);
            case ItemViewType.FOOTER:
                return createFooter(parent);
            case ItemViewType.SUBSECTION_HEADER:
                return createSubsectionHeader(parent);
            default:
                assert false;
                return null;
        }
    }

    @Override
    public final void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        Pair<ItemGroup, Integer> groupAndPosition = getGroupAt(position);
        ItemGroup group = groupAndPosition.first;
        @ItemViewType int viewType = group.getItemViewType(groupAndPosition.second);

        Pair<Date, TimedItem> pair = getItemAt(position);
        switch (viewType) {
            case ItemViewType.DATE:
                ((DateViewHolder) holder).setDate(pair.first);
                break;
            case ItemViewType.NORMAL:
                bindViewHolderForTimedItem(holder, pair.second);
                break;
            case ItemViewType.HEADER:
                bindViewHolderForHeaderItem(holder, (HeaderItem) pair.second);
                break;
            case ItemViewType.FOOTER:
                bindViewHolderForFooterItem(holder, (FooterItem) pair.second);
                break;
            case ItemViewType.SUBSECTION_HEADER:
                bindViewHolderForSubsectionHeader((SubsectionHeaderViewHolder) holder, pair.second);
                break;
        }
    }

    @Override
    public final int getItemCount() {
        return mSize;
    }

    /** Utility method to traverse all groups and find the {@link ItemGroup} for the given position. */
    protected Pair<ItemGroup, Integer> getGroupAt(int position) {
        // TODO(ianwen): Optimize the performance if the number of groups becomes too large.
        int i = position;
        for (ItemGroup group : mGroups) {
            if (i >= group.size()) {
                i -= group.size();
            } else {
                return new Pair<>(group, i);
            }
        }
        assert false;
        return null;
    }

    /** @param item The item to remove from the adapter. */
    // #getGroupAt() asserts false before returning null, causing findbugs to complain about
    // a redundant nullcheck even though getGroupAt can return null.
    protected void removeItem(TimedItem item) {
        Pair<ItemGroup, Integer> groupPair = getGroupAt(item.getPosition());
        if (groupPair == null) {
            Log.e(
                    TAG,
                    "Failed to find group for item during remove. Item position: "
                            + item.getPosition()
                            + ", total size: "
                            + mSize);
            return;
        }

        ItemGroup group = groupPair.first;
        group.removeItem(item);

        // Remove the group if only the date header is left.
        if (group.size() == 1) mGroups.remove(group);

        // Remove header if only the header is left.
        if (hasListHeader() && mGroups.size() == 1) removeHeader();

        setSizeAndGroupPositions();
        notifyDataSetChanged();
    }

    /**
     * Creates a long ID that identifies a particular day in history.
     * @param date Date to process.
     * @return Long that has the day of the year (1-365) in the lowest 16 bits and the year in the
     *         next 16 bits over.
     */
    private static long getStableIdFromDate(Date date) {
        Calendar calendar = CalendarFactory.get();
        calendar.setTime(date);
        long dayOfYear = calendar.get(Calendar.DAY_OF_YEAR);
        long year = calendar.get(Calendar.YEAR);
        return (year << 16) + dayOfYear;
    }

    /**
     * Compares two {@link Date}s. Note if you already have two {@link Calendar} objects, use
     * {@link #compareCalendar(Calendar, Calendar)} instead.
     * @return 0 if date1 and date2 are in the same day; 1 if date1 is before date2; -1 otherwise.
     */
    protected static int compareDate(Date date1, Date date2) {
        Calendar cal1 = CalendarFactory.get();
        Calendar cal2 = CalendarFactory.get();
        cal1.setTime(date1);
        cal2.setTime(date2);
        return compareCalendar(cal1, cal2);
    }

    /** @return 0 if cal1 and cal2 are in the same day; 1 if cal1 happens before cal2; -1 otherwise. */
    private static int compareCalendar(Calendar cal1, Calendar cal2) {
        boolean sameDay =
                cal1.get(Calendar.YEAR) == cal2.get(Calendar.YEAR)
                        && cal1.get(Calendar.DAY_OF_YEAR) == cal2.get(Calendar.DAY_OF_YEAR);
        if (sameDay) {
            return 0;
        } else if (cal1.before(cal2)) {
            return 1;
        } else {
            return -1;
        }
    }

    /** Wraps {@link Calendar#getInstance()} in an {@link AsyncTask} to avoid Strict mode violation. */
    private static AsyncTask<Calendar> createCalendar() {
        return new BackgroundOnlyAsyncTask<Calendar>() {
            @Override
            protected Calendar doInBackground() {
                return Calendar.getInstance();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Calculates the {@link Date} for midnight of the date represented by the |timestamp|. */
    public static Date getDateAtMidnight(long timestamp) {
        Calendar cal = Calendar.getInstance();
        cal.setTimeInMillis(timestamp);
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);
        return cal.getTime();
    }
}
