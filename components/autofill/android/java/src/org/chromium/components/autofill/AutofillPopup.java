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

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.DropdownItemBase;
import org.chromium.ui.DropdownPopupWindow;

import java.util.Arrays;

/** The Autofill suggestion popup that lists relevant suggestions. */
@NullMarked
public class AutofillPopup extends DropdownPopupWindow
        implements AdapterView.OnItemClickListener, PopupWindow.OnDismissListener {
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
     */
    public AutofillPopup(
            Context context,
            View anchorView,
            AutofillDelegate autofillDelegate) {
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
     * Shows the Autofill popup with the given items.
     *
     * @param items Autofill suggestion data.
     * @param isRtl true if right-to-left text.
     */
    @SuppressLint("InlinedApi")
    public void filterAndShow(AutofillDropdownItem[] items, boolean isRtl) {
        setAdapter(new AutofillDropdownAdapter(mContext, Arrays.asList(items)));
        setRtl(isRtl);
        show();
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
        mAutofillDelegate.suggestionSelected(position);
    }

    @Override
    public void onDismiss() {
        mAutofillDelegate.dismissed();
    }

    /** A specialized DropdownItem for Autofill suggestions. */
    public static class AutofillDropdownItem extends DropdownItemBase {
        private final String mLabel;
        private final String mSublabel;

        public AutofillDropdownItem(String label, String sublabel) {
            mLabel = label;
            mSublabel = sublabel;
        }

        @Override
        public String getLabel() {
            return mLabel;
        }

        @Override
        public String getSublabel() {
            return mSublabel;
        }
    }
}
