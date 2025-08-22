// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** A popup for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsPopup {
    private final AnchoredPopupWindow mPopupWindow;
    private final View mContentView;
    /* package */ Button mCameraButton;
    /* package */ Button mGalleryButton;

    NavigationAttachmentsPopup(Context context, View anchorView) {
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.navigation_attachments_popup, null);
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        mPopupWindow =
                new AnchoredPopupWindow(
                        context,
                        anchorView.getRootView(),
                        AppCompatResources.getDrawable(context, R.drawable.menu_bg_baseline),
                        mContentView,
                        rectProvider);
        // `match_parent` and `wrap_content` don't exactly work well in our case.
        // Marking buttons `wrap_content` always narrows down button area, producing inconsistent
        // sizing, and asking for `match_parent` results in text wrapping, as the parent is unable
        // to determine the minimum child size accurately.
        mPopupWindow.setDesiredContentSize(
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.location_bar_navigation_attachments_popup_width),
                0);
        mPopupWindow.setHorizontalOverlapAnchor(true);
        mCameraButton = mContentView.findViewById(R.id.navigation_attachments_camera_button);
        mGalleryButton = mContentView.findViewById(R.id.navigation_attachments_pick_picture_button);
    }

    void show() {
        mPopupWindow.show();
    }

    void dismiss() {
        mPopupWindow.dismiss();
    }

    boolean isShowing() {
        return mPopupWindow.isShowing();
    }
}
