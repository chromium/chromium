// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** A popup for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsPopup {
    private final AnchoredPopupWindow mPopupWindow;
    private final View mContentView;
    /* package */ RecyclerView mTabAttachmentView;
    /* package */ Button mCameraButton;
    /* package */ Button mGalleryButton;
    /* package */ Button mFileButton;
    /* package */ Button mClipboardButton;
    /* package */ TabAttachmentPopupChoicesRecyclerViewAdapter mTabAttachmentsAdapter;
    /* package */ View mRecentTabsHeader;

    NavigationAttachmentsPopup(
            Context context, View anchorView, ModelList tabAttachmentsModelList) {
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
        mTabAttachmentView = mContentView.findViewById(R.id.tab_attachment_recycler_view);
        mCameraButton = mContentView.findViewById(R.id.navigation_attachments_camera_button);
        mGalleryButton = mContentView.findViewById(R.id.navigation_attachments_pick_picture_button);
        mFileButton = mContentView.findViewById(R.id.navigation_attachments_pick_file_button);
        mClipboardButton =
                mContentView.findViewById(R.id.navigation_attachments_paste_from_clipboard_button);
        mRecentTabsHeader =
                mContentView.findViewById(R.id.navigation_attachments_recent_tabs_header);

        mTabAttachmentsAdapter =
                new TabAttachmentPopupChoicesRecyclerViewAdapter(tabAttachmentsModelList);
        mTabAttachmentView.setAdapter(mTabAttachmentsAdapter);
        mTabAttachmentView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
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
