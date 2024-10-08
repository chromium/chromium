// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static android.view.accessibility.AccessibilityNodeInfo.CollectionInfo.SELECTION_MODE_SINGLE;

import android.app.AlertDialog;
import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.LinearLayout;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is to inflate the layout and set all the buttons, views and texts within the layout.
 * It sets the color and columns and also handles the button difference between the simple and
 * advanced views.
 */
public class ColorPickerDialogView extends AlertDialog implements OnColorChangedListener {
    // Callbacks to handle user interactions (picking a color, switching views, and closing dialog).
    private Callback<Integer> mCustomColorPickedCallback;
    private Callback<Void> mViewSwitchedCallback;
    private Callback<Boolean> mMakeChoiceCallback;
    private Callback<Integer> mDialogDismissedCallback;

    // GridView of the suggested colors from the web dev (or the default list if empty).
    private ColorPickerSuggestionsView mSuggestionsView;

    // View elements.
    private View mDialogContent;
    private View mChosenColor;
    private LinearLayout mChosenColorContainer;
    private ButtonCompat mViewSwitcher;
    private ColorPickerAdvanced mCustomView;

    public ColorPickerDialogView(Context context) {
        super(context);

        // Inflate dialog content and set the title.
        mDialogContent =
                LayoutInflaterUtils.inflate(context, R.layout.color_picker_dialog_view, null);
        setView(mDialogContent);
        setTitle(context.getString(R.string.color_picker_dialog_title));

        // Set other view elements and their listeners.
        mSuggestionsView =
                (ColorPickerSuggestionsView)
                        mDialogContent.findViewById(R.id.color_picker_suggestions_view);
        mChosenColor = mDialogContent.findViewById(R.id.color_picker_dialog_chosen_color_view);
        mChosenColorContainer =
                mDialogContent.findViewById(R.id.color_picker_dialog_chosen_color_container);

        mCustomView =
                (ColorPickerAdvanced) mDialogContent.findViewById(R.id.color_picker_custom_view);
        mCustomView.setListener(this);

        mViewSwitcher =
                (ButtonCompat) mDialogContent.findViewById(R.id.color_picker_view_switcher_button);
        mViewSwitcher.setOnClickListener(v -> mViewSwitchedCallback.onResult(null));

        // Create a positive and negative button to cancel/set.
        setButton(
                BUTTON_POSITIVE,
                context.getString(R.string.color_picker_button_set),
                (dialog, which) -> {
                    mMakeChoiceCallback.onResult(true);
                });
        setButton(
                BUTTON_NEGATIVE,
                context.getString(R.string.color_picker_button_cancel),
                (dialog, which) -> {
                    mMakeChoiceCallback.onResult(false);
                });
        setOnCancelListener(
                dialog ->
                        mDialogDismissedCallback.onResult(
                                ((ColorDrawable) mChosenColor.getBackground()).getColor()));
    }

    void setDialogDismissedCallback(Callback<Integer> dialogDismissedCallback) {
        mDialogDismissedCallback = dialogDismissedCallback;
    }

    void switchViewType(boolean isCustom) {
        if (isCustom) {
            mViewSwitcher.setText(R.string.color_picker_button_suggestions);
            mSuggestionsView.setVisibility(View.GONE);
            mCustomView.setVisibility(View.VISIBLE);
        } else {
            mViewSwitcher.setText(R.string.color_picker_button_custom);
            mSuggestionsView.setVisibility(View.VISIBLE);
            mCustomView.setVisibility(View.GONE);
        }
    }

    void setNumberOfColumns(int numOfColumns) {
        mSuggestionsView.setNumColumns(numOfColumns);
    }

    void setSuggestionsAdapter(ModelListAdapter adapter) {
        mSuggestionsView.setAdapter(adapter);
        mSuggestionsView.setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.setCollectionInfo(
                                AccessibilityNodeInfo.CollectionInfo.obtain(
                                        adapter.getCount(), 1, false, SELECTION_MODE_SINGLE));
                        info.setText(
                                getContext()
                                        .getResources()
                                        .getString(R.string.color_picker_button_suggestions));
                        info.setClassName(ListView.class.getName());
                    }
                });
    }

    void setColor(int newColor) {
        mChosenColor.setBackgroundColor(newColor);
        // Set an updated content description on the chosen color container.
        String hexColor = String.format("#%06X", (0xFFFFFF & newColor));
        mChosenColorContainer.setContentDescription(
                getContext().getResources().getString(R.string.color_picker_button_suggestions)
                        + hexColor);
    }

    void setCustomColorPickedCallback(Callback<Integer> callback) {
        mCustomColorPickedCallback = callback;
    }

    void setViewSwitchedCallback(Callback<Void> callback) {
        mViewSwitchedCallback = callback;
    }

    void setMakeChoiceCallback(Callback<Boolean> callback) {
        mMakeChoiceCallback = callback;
    }

    @Override
    public void onColorChanged(int color) {
        mCustomColorPickedCallback.onResult(color);
    }

    @VisibleForTesting
    View getContentView() {
        return mDialogContent;
    }
}
