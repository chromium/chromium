// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.PopupWindow;

import org.chromium.ui.DropdownItem;
import org.chromium.ui.DropdownPopupWindow;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * The Autofill suggestion popup that lists relevant suggestions.
 */
public class AutofillPopup extends DropdownPopupWindow
        implements AdapterView.OnItemClickListener, AdapterView.OnItemLongClickListener,
                   PopupWindow.OnDismissListener, AutofillDropdownFooter.Observer {
    /**
     * We post a delayed runnable to clear accessibility focus from the autofill popup's list view
     * when we receive a {@code TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED} event because we receive a
     * {@code TYPE_VIEW_ACCESSIBILITY_FOCUSED} for the same list view if user navigates to a
     * different suggestion. On the other hand, if user navigates out of the popup we do not receive
     * a {@code TYPE_VIEW_ACCESSIBILITY_FOCUSED} in immediate succession.
     */
    private static final long CLEAR_ACCESSIBILITY_FOCUS_DELAY_MS = 100;

    private final Context mContext;
    private final AutofillDelegate mAutofillDelegate;
    private List<AutofillSuggestion> mSuggestions;

    private final Runnable mClearAccessibilityFocusRunnable = new Runnable() {
        @Override
        public void run() {
            mAutofillDelegate.accessibilityFocusCleared();
        }
    };

    /**
     * Creates an AutofillWindow with specified parameters.
     * @param context Application context.
     * @param anchorView View anchored for popup.
     * @param autofillDelegate An object that handles the calls to the native AutofillPopupView.
     */
    public AutofillPopup(Context context, View anchorView, AutofillDelegate autofillDelegate) {
        super(context, anchorView);
        mContext = context;
        mAutofillDelegate = autofillDelegate;

        setOnItemClickListener(this);
        setOnDismissListener(this);
        disableHideOnOutsideTap();
        setContentDescriptionForAccessibility(
                mContext.getString(R.string.autofill_popup_content_description));
    }

    /**
     * Filters the Autofill suggestions to the ones that we support and shows the popup.
     * @param suggestions Autofill suggestion data.
     * @param isRtl @code true if right-to-left text.
     * @param isRefresh Whether or not refreshed visual style should be used.
     */
    @SuppressLint("InlinedApi")
    public void filterAndShow(AutofillSuggestion[] suggestions, boolean isRtl, boolean isRefresh) {
        mSuggestions = new ArrayList<AutofillSuggestion>(Arrays.asList(suggestions));
        // Remove the AutofillSuggestions with IDs that are not supported by Android
        List<DropdownItem> cleanedData = new ArrayList<>();
        List<DropdownItem> footerRows = new ArrayList<>();
        HashSet<Integer> separators = new HashSet<Integer>();
        for (int i = 0; i < suggestions.length; i++) {
            int itemId = suggestions[i].getSuggestionId();
            if (itemId == PopupItemId.ITEM_ID_SEPARATOR) {
                separators.add(cleanedData.size());
            } else if (isFooter(itemId, isRefresh)) {
                footerRows.add(suggestions[i]);
            } else {
                cleanedData.add(suggestions[i]);
            }
        }

        // TODO(crbug.com/896349): Ideally, we would set the footer each time, as this guard assumes
        // the footer is unchanged between calls to filterAndShow. However, the JellyBean popup
        // implementation will not draw footers added after the initial call to show().
        if (!footerRows.isEmpty() && !isShowing()) {
            setFooterView(new AutofillDropdownFooter(mContext, footerRows, this));
        }

        setAdapter(new AutofillDropdownAdapter(mContext, cleanedData, separators, isRefresh));
        setRtl(isRtl);
        show();
        getListView().setOnItemLongClickListener(this);
        getListView().setAccessibilityDelegate(new AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                getListView().removeCallbacks(mClearAccessibilityFocusRunnable);
                if (event.getEventType()
                        == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED) {
                    getListView().postDelayed(
                            mClearAccessibilityFocusRunnable, CLEAR_ACCESSIBILITY_FOCUS_DELAY_MS);
                }
                return super.onRequestSendAccessibilityEvent(host, child, event);
            }
        });
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        AutofillDropdownAdapter adapter = (AutofillDropdownAdapter) parent.getAdapter();
        int listIndex = mSuggestions.indexOf(adapter.getItem(position));
        assert listIndex > -1;
        mAutofillDelegate.suggestionSelected(listIndex);
    }

    @Override
    public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
        AutofillDropdownAdapter adapter = (AutofillDropdownAdapter) parent.getAdapter();
        AutofillSuggestion suggestion = (AutofillSuggestion) adapter.getItem(position);
        if (!suggestion.isDeletable()) return false;

        int listIndex = mSuggestions.indexOf(suggestion);
        assert listIndex > -1;
        mAutofillDelegate.deleteSuggestion(listIndex);
        return true;
    }

    @Override
    public void onDismiss() {
        mAutofillDelegate.dismissed();
    }

    @Override
    public void onFooterSelection(DropdownItem item) {
        // TODO(crbug.com/896349): Finding the suggestion index by its frontend id is a workaround
        // for the fact that footer items are not redrawn on each call to filterAndShow, and so
        // |item| will be identical to, but not equal to, an element in |mSuggestions|. Once this
        // workaround is no longer needed, this should be changed to simply use
        // mSuggestions.indexOf(item).
        int index = -1;

        for (int i = 0; i < mSuggestions.size(); i++) {
            // Cast from DropdownItem to AutofillSuggestion is safe because filterAndShow creates
            // the AutofillDropdownFooter which invokes this, and passes an AutofillSuggestion to
            // the constructor.
            if ((mSuggestions.get(i).getSuggestionId()
                        == ((AutofillSuggestion) item).getSuggestionId())) {
                index = i;
                break;
            }
        }

        assert index > -1;
        mAutofillDelegate.suggestionSelected(index);
    }

    private boolean isFooter(int row, boolean isRefresh) {
        // Footer items are only handled as a special case in the refreshed UI.
        if (!isRefresh) {
            return false;
        }

        switch (row) {
            case PopupItemId.ITEM_ID_CLEAR_FORM:
            case PopupItemId.ITEM_ID_AUTOFILL_OPTIONS:
            case PopupItemId.ITEM_ID_SCAN_CREDIT_CARD:
            case PopupItemId.ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
            case PopupItemId.ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
                return true;
            default:
                return false;
        }
    }
}
