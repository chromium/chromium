// Copyright 2013 The Chromium Authors
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

import androidx.annotation.Nullable;

import org.chromium.ui.DropdownItem;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** The Autofill suggestion popup that lists relevant suggestions. */
public class AutofillPopup extends DropdownPopupWindow
        implements AdapterView.OnItemClickListener,
                AdapterView.OnItemLongClickListener,
                PopupWindow.OnDismissListener {
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

    private final Runnable mClearAccessibilityFocusRunnable =
            new Runnable() {
                @Override
                public void run() {
                    mAutofillDelegate.accessibilityFocusCleared();
                }
            };

    /**
     * Creates an AutofillWindow with specified parameters.
     *
     * @param context Application context.
     * @param anchorView View anchored for popup.
     * @param autofillDelegate An object that handles the calls to the native AutofillPopupView.
     * @param visibleWebContentsRectProvider The {@link RectProvider} for popup limits.
     */
    public AutofillPopup(
            Context context,
            View anchorView,
            AutofillDelegate autofillDelegate,
            @Nullable RectProvider visibleWebContentsRectProvider) {
        super(context, anchorView, visibleWebContentsRectProvider);
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
     *
     * @param suggestions Autofill suggestion data.
     * @param isRtl true if right-to-left text.
     */
    @SuppressLint("InlinedApi")
    public void filterAndShow(AutofillSuggestion[] suggestions, boolean isRtl) {
        mSuggestions = new ArrayList<AutofillSuggestion>(Arrays.asList(suggestions));
        // Remove the AutofillSuggestions with IDs that are not supported by Android
        List<DropdownItem> cleanedData = new ArrayList<>();
        HashSet<Integer> separators = new HashSet<Integer>();
        for (int i = 0; i < suggestions.length; i++) {
            int itemId = suggestions[i].getSuggestionType();
            if (itemId == SuggestionType.SEPARATOR) {
                separators.add(cleanedData.size());
            } else {
                cleanedData.add(suggestions[i]);
            }
        }

        setAdapter(new AutofillDropdownAdapter(mContext, cleanedData, separators));
        setRtl(isRtl);
        show();
        getListView().setOnItemLongClickListener(this);
        getListView()
                .setAccessibilityDelegate(
                        new AccessibilityDelegate() {
                            @Override
                            public boolean onRequestSendAccessibilityEvent(
                                    ViewGroup host, View child, AccessibilityEvent event) {
                                getListView().removeCallbacks(mClearAccessibilityFocusRunnable);
                                if (event.getEventType()
                                        == AccessibilityEvent
                                                .TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED) {
                                    getListView()
                                            .postDelayed(
                                                    mClearAccessibilityFocusRunnable,
                                                    CLEAR_ACCESSIBILITY_FOCUS_DELAY_MS);
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
}
