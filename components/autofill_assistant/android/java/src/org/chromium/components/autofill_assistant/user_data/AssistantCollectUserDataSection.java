// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.components.autofill_assistant.AssistantEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel;
import org.chromium.components.autofill_assistant.AssistantTagsForTesting;
import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;

import java.util.ArrayList;
import java.util.List;

/**
 * This is the generic superclass for all autofill-assistant payment request sections.
 *
 * @param <T> The type of entry that a concrete instance of this class is created for,
 *            such as {@link AssistantAutofillProfile}, {@link AssistantPaymentInstrument}, etc.
 */
public abstract class AssistantCollectUserDataSection<T extends AssistantOptionModel> {
    interface Delegate<T> {
        void onUserDataChanged(T item, @AssistantUserDataEventType int type);
    }

    private class Item {
        View mFullView;
        T mOption;

        Item(View fullView, T option) {
            this.mFullView = fullView;
            this.mOption = option;
        }
    }

    protected final Context mContext;
    private final @Nullable View mTitleAddButton;
    private final AssistantVerticalExpander mSectionExpander;
    private final AssistantChoiceList mItemsView;
    private final View mSummaryContentView;
    private final View mSummarySpinnerView;
    private final int mFullViewResId;
    private final int mTitleToContentPadding;
    private final List<Item> mItems;

    private boolean mUiEnabled = true;
    private boolean mIsLoading;
    private boolean mIgnoreItemSelectedNotifications;
    private boolean mIgnoreItemChangeNotification;
    private boolean mRequestReloadOnChange;
    private @Nullable Delegate<T> mDelegate;
    private int mTopPadding;
    private int mBottomPadding;

    protected T mSelectedOption;

    /**
     *
     * @param context The context to use.
     * @param parent The parent view to add this payment request section to.
     * @param summaryViewResId The resource ID of the summary view to inflate.
     * @param fullViewResId The resource ID of the full view to inflate.
     * @param titleToContentPadding The amount of padding between title and content views.
     * @param titleAddButton The string to display in the title add button. Can be null if no add
     *         button should be created.
     * @param listAddButton The string to display in the add button at the bottom of the list. Can
     *         be null if no add button should be created.
     */
    public AssistantCollectUserDataSection(Context context, ViewGroup parent, int summaryViewResId,
            int fullViewResId, int titleToContentPadding, @Nullable String titleAddButton,
            @Nullable String listAddButton) {
        mContext = context;
        mFullViewResId = fullViewResId;
        mItems = new ArrayList<>();
        mTitleToContentPadding = titleToContentPadding;

        LayoutInflater inflater = LayoutUtils.createInflater(context);
        mSectionExpander = new AssistantVerticalExpander(context, null);
        View sectionTitle =
                inflater.inflate(R.layout.autofill_assistant_payment_request_section_title, null);
        mSummaryContentView = inflater.inflate(summaryViewResId, /* root= */ null);
        mSummarySpinnerView =
                inflater.inflate(R.layout.autofill_assistant_loading_spinner, /* root= */ null);
        View summaryView = buildSummaryView(context, mSummaryContentView, mSummarySpinnerView);
        mItemsView = createChoiceList(listAddButton);

        mSectionExpander.setTitleView(sectionTitle,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setCollapsedView(summaryView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setExpandedView(mItemsView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Adjust margins such that title and collapsed views are indented, but expanded view is
        // full-width.
        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        setHorizontalMargins(sectionTitle, horizontalMargin, horizontalMargin);
        setHorizontalMargins(mSectionExpander.getChevronButton(), 0, horizontalMargin);
        setHorizontalMargins(summaryView, horizontalMargin, 0);
        setHorizontalMargins(mItemsView, 0, 0);

        if (titleAddButton == null) {
            mSectionExpander.findViewById(R.id.section_title_add_button).setVisibility(View.GONE);
            mTitleAddButton = null;
        } else {
            mTitleAddButton = mSectionExpander.findViewById(R.id.section_title_add_button);
            TextView titleAddButtonLabelView =
                    mSectionExpander.findViewById(R.id.section_title_add_button_label);
            titleAddButtonLabelView.setText(titleAddButton);
            ImageView titleAddButtonIconView =
                    mSectionExpander.findViewById(R.id.section_title_add_button_icon);
            ApiCompatibilityUtils.setImageTintList(titleAddButtonIconView,
                    ContextCompat.getColorStateList(mContext, R.color.blue_when_enabled_list));
            mTitleAddButton.setOnClickListener(unusedView -> createOrEditItem(null));
        }

        parent.addView(mSectionExpander,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        updateUi();
    }

    View getView() {
        return mSectionExpander;
    }

    /**
     * Set the view itself visible, if allowed. This can be overruled by the view deciding that
     * it has no reason to be visible, by being empty (no items) and not having an editor.
     *
     * @param visible The flag to decide the visibility.
     */
    void setVisible(boolean visible) {
        mSectionExpander.setVisibility(visible && canBeVisible() ? View.VISIBLE : View.GONE);
    }

    void setDelegate(@Nullable Delegate<T> delegate) {
        mDelegate = delegate;
    }

    void setTitle(String title) {
        TextView titleView = mSectionExpander.findViewById(R.id.section_title);
        AssistantTextUtils.applyVisualAppearanceTags(titleView, title, null);
    }

    /**
     * Replaces the set of displayed items.
     *
     * @param options The new items.
     * @param selectedItemIndex The index of the item in |items| to select.
     */
    void setItems(List<T> options, int selectedItemIndex) {
        mItems.clear();
        mItemsView.clearItems();
        mSelectedOption = null;
        Item initiallySelectedItem = null;
        for (int i = 0; i < options.size(); i++) {
            Item item = createItem(options.get(i));
            addItem(item);

            if (i == selectedItemIndex) {
                initiallySelectedItem = item;
            }
        }
        updateUi();

        if (initiallySelectedItem != null) {
            selectItem(initiallySelectedItem, false, AssistantUserDataEventType.NO_NOTIFICATION);
        }
    }

    /**
     * Returns the list of items.
     */
    List<T> getItems() {
        List<T> items = new ArrayList<>();
        for (Item item : mItems) {
            items.add(item.mOption);
        }
        return items;
    }

    /**
     * Manually updates the summary and all full views. Should be called by subclasses after a
     * change to how items are displayed in summary or full views.
     */
    void updateViews() {
        if (mSelectedOption != null) {
            updateSummaryView(mSummaryContentView, mSelectedOption);
        }
        for (int i = 0; i < mItems.size(); i++) {
            updateFullView(mItems.get(i).mFullView, mItems.get(i).mOption);
        }
    }

    /**
     * Adds a new item to the list, or updates an item in-place if it is already in the list.
     *
     * @param option The item to add or update.
     * @param select Whether to select the new/updated item or not.
     * @param notify Whether to notify the controller of this change or not.
     */
    void addOrUpdateItem(@Nullable T option, boolean select, boolean notify) {
        if (option == null) {
            return;
        }

        // Update existing item if possible.
        Item item = null;
        for (int i = 0; i < mItems.size(); i++) {
            if (areEqual(mItems.get(i).mOption, option)) {
                item = mItems.get(i);
                item.mOption = option;
                updateFullView(item.mFullView, item.mOption);
                break;
            }
        }

        @AssistantUserDataEventType
        int eventType;
        if (item == null) {
            eventType = AssistantUserDataEventType.ENTRY_CREATED;
            item = createItem(option);
            addItem(item);
        } else {
            eventType = AssistantUserDataEventType.ENTRY_EDITED;
            updateSummaryView(mSummaryContentView, item.mOption);
        }

        if (select) {
            selectItem(item, notify, eventType);
        }
    }

    void setPaddings(int topPadding, int bottomPadding) {
        mTopPadding = topPadding;
        mBottomPadding = bottomPadding;
        updatePaddings();
    }

    void setRequestReloadOnChange(boolean requestReloadOnChange) {
        mRequestReloadOnChange = requestReloadOnChange;
    }

    private View buildSummaryView(Context context, View contentView, View spinnerView) {
        LinearLayout viewWrapper = new LinearLayout(context);

        viewWrapper.addView(contentView);

        spinnerView.setTag(AssistantTagsForTesting.COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG);
        spinnerView.setVisibility(View.GONE);
        viewWrapper.addView(spinnerView);

        return viewWrapper;
    }

    private AssistantChoiceList createChoiceList(@Nullable String addButtonText) {
        AssistantChoiceList list = new AssistantChoiceList(mContext, /* attrs= */ null,
                addButtonText, /* rowSpacingInPixels= */ 0,
                mContext.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_column_spacing),
                /* layoutHasEditButton= */ true);
        int verticalPadding = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_choice_top_bottom_padding);
        list.setPadding(mContext.getResources().getDimensionPixelSize(
                                R.dimen.autofill_assistant_bottombar_horizontal_spacing),
                verticalPadding,
                mContext.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_choice_list_padding_end),
                verticalPadding);
        list.setBackgroundColor(mContext.getColor(R.color.omnibox_bg_color));
        list.setTag(AssistantTagsForTesting.COLLECT_USER_DATA_CHOICE_LIST);
        if (addButtonText != null) {
            list.setOnAddButtonClickedListener(() -> createOrEditItem(null));
        }
        return list;
    }

    private void updatePaddings() {
        View titleView = mSectionExpander.getTitleView();
        if (isEmpty() && !mIsLoading) {
            // Section is empty, i.e., the title is the bottom-most widget.
            titleView.setPadding(titleView.getPaddingLeft(), mTopPadding,
                    titleView.getPaddingRight(), mBottomPadding);
        } else if (mSectionExpander.isExpanded()) {
            // Section is expanded, i.e., the expanded widget is the bottom-most widget.
            titleView.setPadding(titleView.getPaddingLeft(), mTopPadding,
                    titleView.getPaddingRight(), mTitleToContentPadding);
            // No need to set additional bottom padding, expanded sections have enough already.
        } else {
            // Section is non-empty and collapsed -> collapsed widget is the bottom-most widget.
            titleView.setPadding(titleView.getPaddingLeft(), mTopPadding,
                    titleView.getPaddingRight(), mTitleToContentPadding);
            setBottomPadding(mSectionExpander.getCollapsedView(), mBottomPadding);
        }
    }

    private void setBottomPadding(View view, int padding) {
        view.setPadding(
                view.getPaddingLeft(), view.getPaddingTop(), view.getPaddingRight(), padding);
    }

    /**
     * Creates a new item from {@code option}.
     */
    private Item createItem(T option) {
        View fullView = LayoutUtils.createInflater(mContext).inflate(mFullViewResId, null);
        updateFullView(fullView, option);
        Item item = new Item(fullView, option);
        return item;
    }

    /**
     * Adds {@code item} to the UI.
     */
    private void addItem(Item item) {
        mItems.add(item);
        boolean canEditOption = item.mOption.canEdit();
        @DrawableRes
        int editButtonDrawable = R.drawable.ic_edit_24dp;
        String editButtonContentDescription = "";
        if (canEditOption) {
            editButtonDrawable = getEditButtonDrawable(item.mOption);
            editButtonContentDescription = getEditButtonContentDescription(item.mOption);
        }
        mItemsView.addItem(item.mFullView, /* hasEditButton= */ canEditOption,
                /* itemSelectedListener= */
                selected
                -> {
                    if (mIgnoreItemSelectedNotifications || !selected) {
                        return;
                    }
                    selectItem(item, /*notify=*/true, AssistantUserDataEventType.SELECTION_CHANGED);
                    if (item.mOption.isComplete()) {
                        // Workaround for Android bug: a layout transition may cause the newly
                        // checked radiobutton to not render properly.
                        mSectionExpander.post(() -> mSectionExpander.setExpanded(false));
                    } else {
                        createOrEditItem(item.mOption);
                    }
                },
                /* itemEditedListener= */
                ()
                        -> createOrEditItem(item.mOption),
                /* editButtonDrawable= */ editButtonDrawable,
                /* editButtonContentDescription= */ editButtonContentDescription);
        updateUi();
    }

    private void selectItem(Item item, boolean notify, @AssistantUserDataEventType int eventType) {
        mSelectedOption = item.mOption;
        mIgnoreItemSelectedNotifications = true;
        mItemsView.setCheckedItem(item.mFullView);
        mIgnoreItemSelectedNotifications = false;
        updateSummaryView(mSummaryContentView, item.mOption);
        updateUi();

        if (notify) {
            notifyDataChanged(item.mOption, eventType);
        }
    }

    /**
     * Asks the subclass to update the contents of {@code fullView}, which was previously created by
     * {@code createFullView}.
     */
    protected abstract void updateFullView(View fullView, T option);

    /** Asks the subclass to update the contents of the summary view. */
    protected abstract void updateSummaryView(View summaryView, T option);

    /** Asks the subclass which drawable to use for {@code option}. */
    protected abstract @DrawableRes int getEditButtonDrawable(T option);

    /** Asks the subclass for the content description of {@code option}. */
    protected abstract String getEditButtonContentDescription(T option);

    /** Ask the subclass if two {@code option} instances should be considered equal. */
    protected abstract boolean areEqual(@Nullable T optionA, @Nullable T optionB);

    protected boolean shouldIgnoreItemChangeNotification() {
        return mIgnoreItemChangeNotification;
    }

    /**
     * Get the current editor. Defaults to null. Subclasses may override this method to provide
     * their owned editor.
     * @return The current editor.
     */
    @Nullable
    protected AssistantEditor<T> getEditor() {
        return null;
    }

    /**
     * Asks the subclass for an editor to edit an item or create a new one (if {@code oldItem} is
     * null). Calls {@code addOrUpdateItem} after a successful edit or sends a reload notification
     * if requested.
     * Subclasses may override this if they want to specify their own behaviour.
     * @param oldItem The item to be edited ({@code null} if a new item should be created).
     */
    protected void createOrEditItem(@Nullable T oldItem) {
        AssistantEditor<T> editor = getEditor();
        if (editor == null) {
            return;
        }

        Callback<T> doneCallback = editedItem -> {
            if (mRequestReloadOnChange) {
                int eventType = oldItem == null ? AssistantUserDataEventType.ENTRY_CREATED
                                                : AssistantUserDataEventType.ENTRY_EDITED;
                mSectionExpander.post(() -> mSectionExpander.setExpanded(false));
                notifyDataChanged(editedItem, eventType);
                return;
            }

            mIgnoreItemChangeNotification = true;
            addOrUpdateItem(editedItem,
                    /* select= */ true, /* notify= */ true);
            mIgnoreItemChangeNotification = false;
        };

        Callback<T> cancelCallback = ignoredItem -> {};

        editor.createOrEditItem(oldItem, doneCallback, cancelCallback);
    }

    private void notifyDataChanged(@Nullable T item, @AssistantUserDataEventType int eventType) {
        if (mDelegate == null) {
            return;
        }
        mDelegate.onUserDataChanged(item, eventType);
    }

    /**
     * For convenience. Hides {@code view} if it is empty.
     */
    void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }

    private boolean isEmpty() {
        return mItems.isEmpty();
    }

    private void setHorizontalMargins(View view, int marginStart, int marginEnd) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(marginStart);
        lp.setMarginEnd(marginEnd);
        view.setLayoutParams(lp);
    }

    /**
     * Enable or disable UI interactions.
     * @param enabled The flag to disable the interactions.
     */
    void setEnabled(boolean enabled) {
        mUiEnabled = enabled;
        updateUi();
    }

    /**
     * Set the view to loading, showing a spinner instead of the content - or to done, showing the
     * content while hiding the spinner.
     *
     * @param loading The flag.
     */
    void setLoading(boolean loading) {
        mIsLoading = loading;
        mSummaryContentView.setVisibility(loading ? View.GONE : View.VISIBLE);
        mSummarySpinnerView.setVisibility(loading ? View.VISIBLE : View.GONE);
        AssistantLoadingSpinner loadingSpinner =
                mSummarySpinnerView.findViewById(R.id.loading_spinner);
        loadingSpinner.setAnimationState(loading);
        updatePaddings();
    }

    /**
     * Update the UI if something changed.
     */
    protected void updateUi() {
        boolean hasEditor = getEditor() != null;
        if (mTitleAddButton != null) {
            mTitleAddButton.setVisibility(isEmpty() && hasEditor ? View.VISIBLE : View.GONE);
            setTitleAddButtonEnabled(mUiEnabled);
        }
        mItemsView.setAddButtonVisible(/* visible= */ hasEditor);
        mItemsView.setUiEnabled(mUiEnabled);
        mSectionExpander.setFixed(isEmpty());
        mSectionExpander.setCollapsedVisible(!isEmpty());
        mSectionExpander.setExpandedVisible(!isEmpty());
        if (isEmpty()) {
            mSectionExpander.setExpanded(false);
        }
        updatePaddings();
    }

    private boolean canBeVisible() {
        return !isEmpty() || getEditor() != null;
    }

    private void setTitleAddButtonEnabled(boolean enabled) {
        assert mTitleAddButton != null;
        mTitleAddButton.setEnabled(enabled);
        mTitleAddButton.findViewById(R.id.section_title_add_button_icon).setEnabled(enabled);
        mTitleAddButton.findViewById(R.id.section_title_add_button_label).setEnabled(enabled);
    }
}
