// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;

import java.util.List;

/**
 * The view portion of the PWA Install bottom sheet.
 *
 * <p>The class is suppressing lint error for 'missing performClick override' because we don't need
 * the override. We're forwarding the event along (and the implementation of the override would be a
 * no-op for us).
 */
@SuppressLint("ClickableViewAccessibility")
public class PwaRestoreBottomSheetView implements View.OnTouchListener {
    private static final int APP_ICON_SIZE_DP = 40;
    private static final int APP_ICON_CORNER_RADIUS_DP = 20;
    private static final int APP_ICON_TEXT_SIZE_DP = 24;

    // The current context.
    private final Context mContext;

    // The peek state for the bottom sheet.
    private View mPreviewView;

    // The details of the bottom sheet.
    private View mContentView;

    // The icon generator for the app icon placeholders.
    private RoundedIconGenerator mIconGenerator;

    // The listener to notify when the Back button is clicked.
    private OnClickListener mBackButtonListener;

    // The listener to notify when an app checkbox is toggled in the app list.
    private OnClickListener mSelectionToggleButtonListener;

    // The back button arrow in the top bar of the content view.
    private Drawable mBackArrow;

    public PwaRestoreBottomSheetView(Context context) {
        mContext = context;
    }

    public void initialize(int backArrowId) {
        mPreviewView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.pwa_restore_bottom_sheet_preview, /* root= */ null);
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.pwa_restore_bottom_sheet_content, /* root= */ null);

        int backgroundId = R.drawable.pwa_restore_icon;
        mPreviewView.findViewById(R.id.icon).setBackgroundResource(backgroundId);
        mPreviewView.findViewById(R.id.icon).setTag(backgroundId);
        mBackArrow =
                backArrowId != 0
                        ? ResourcesCompat.getDrawable(
                                mContext.getResources(), backArrowId, mContext.getTheme())
                        : null;
        TextView contentViewTitle = (TextView) mContentView.findViewById(R.id.title);
        contentViewTitle.setCompoundDrawablesRelativeWithIntrinsicBounds(
                mBackArrow, null, null, null);
        contentViewTitle.setOnTouchListener(this::onTouch);

        int iconColor = mContext.getColor(R.color.default_favicon_background_color);
        mIconGenerator =
                new RoundedIconGenerator(
                        mContext.getResources(),
                        APP_ICON_SIZE_DP,
                        APP_ICON_SIZE_DP,
                        APP_ICON_CORNER_RADIUS_DP,
                        iconColor,
                        APP_ICON_TEXT_SIZE_DP);
    }

    public View getContentView() {
        return mContentView;
    }

    public View getPreviewView() {
        return mPreviewView;
    }

    public void setDisplayedView(@ViewState int viewState) {
        mPreviewView.setVisibility(viewState == ViewState.VIEW_PWA_LIST ? View.GONE : View.VISIBLE);
        mContentView.setVisibility(viewState == ViewState.PREVIEW ? View.GONE : View.VISIBLE);
    }

    protected void setAppList(
            List<PwaRestoreProperties.AppInfo> appList, String recentAppLabel, String oldAppLabel) {
        LinearLayout scrollViewContent = getContentView().findViewById(R.id.scroll_view_content);
        scrollViewContent.removeAllViews();

        // Set the heading for the app list.
        View label =
                LayoutInflater.from(mContext).inflate(R.layout.pwa_restore_list_item_label, null);
        ((TextView) label.findViewById(R.id.label_text)).setText(recentAppLabel);
        scrollViewContent.addView(label);

        int item = 0;
        for (PwaRestoreProperties.AppInfo app : appList) {
            View appView =
                    LayoutInflater.from(mContext).inflate(R.layout.pwa_restore_list_item_app, null);

            // Apply the right background to the item (first item has rounded corner on top, last
            // item has rounded corners on bottom, and all items in between have no rounded
            // corners).
            if (item == 0) {
                appView.setBackgroundResource(R.drawable.pwa_restore_app_item_background_top);
            } else {
                appView.setBackgroundResource(
                        (item == appList.size() - 1)
                                ? R.drawable.pwa_restore_app_item_background_bottom
                                : R.drawable.pwa_restore_app_item_background_middle);
            }
            item += 1;

            // TODO(finnur): Replace with actual app icons.
            Bitmap placeholder = mIconGenerator.generateIconForText(app.getName());
            ((ImageView) appView.findViewById(R.id.app_icon)).setImageBitmap(placeholder);
            ((TextView) appView.findViewById(R.id.app_name)).setText(app.getName());
            CheckBox checkBox = (CheckBox) appView.findViewById(R.id.checkbox);
            checkBox.setTag(app.getId());
            checkBox.setChecked(app.isSelected());
            checkBox.setOnClickListener(this::onClick);

            // Any click on an app item, that is not handled by the view itself, should be treated
            // as an attempt to toggle the checkbox.
            appView.setOnClickListener(this::onClick);

            scrollViewContent.addView(appView);

            // Add a 4dp separator view as a separate item in the ScrollView so as to not affect the
            // height of the app item view (or mess up the rounded corners).
            View separator = new View(mContext);
            separator.setLayoutParams(
                    new LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            (int)
                                    TypedValue.applyDimension(
                                            TypedValue.COMPLEX_UNIT_DIP,
                                            4,
                                            mContext.getResources().getDisplayMetrics())));
            scrollViewContent.addView(separator);
        }
    }

    public void onClick(View view) {
        CheckBox checkBox = null;
        if (view instanceof CheckBox) {
            checkBox = (CheckBox) view;
        } else {
            // Clicks outside the checkbox, that are not handled by the corresponding view, are
            // forwarded to the checkbox.
            checkBox = (CheckBox) view.findViewById(R.id.checkbox);
            checkBox.toggle();
        }

        // Notify of the change.
        mSelectionToggleButtonListener.onClick(checkBox);
    }

    protected void setBackButtonListener(OnClickListener listener) {
        mBackButtonListener = listener;
    }

    protected void setSelectionToggleButtonListener(OnClickListener listener) {
        mSelectionToggleButtonListener = listener;
    }

    // Called through the {@link PwaRestoreBottomSheetViewBinder} bindings when the property model
    // updates:

    int getPeekHeight() {
        return mPreviewView.getHeight();
    }

    @Override
    public boolean onTouch(View view, MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            MarginLayoutParams lp = (MarginLayoutParams) view.getLayoutParams();
            // Allow for a bit of margin for the hit boundary.
            if ((event.getRawX() <= mBackArrow.getIntrinsicWidth() + (2 * lp.leftMargin))) {
                return true;
            }
        } else if (event.getAction() == MotionEvent.ACTION_UP) {
            mBackButtonListener.onClick(view);
            return true;
        }
        return false;
    }
}
