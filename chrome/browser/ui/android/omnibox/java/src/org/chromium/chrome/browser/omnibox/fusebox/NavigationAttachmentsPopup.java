// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;
import android.view.View;
import android.widget.Button;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** A popup for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsPopup {
    private final AnchoredPopupWindow mPopupWindow;
    private final View mContentView;
    /* package */ final Button mAddCurrentTab;
    /* package */ final Button mTabButton;
    /* package */ final Button mCameraButton;
    /* package */ final Button mGalleryButton;
    /* package */ final Button mFileButton;
    /* package */ final Button mClipboardButton;
    /* package */ final Button mAiModeButton;
    /* package */ final View mAutocompleteRequestTypeGroup;

    NavigationAttachmentsPopup(Context context, AnchoredPopupWindow popupWindow, View contentView) {
        mPopupWindow = popupWindow;
        mContentView = contentView;
        // `match_parent` and `wrap_content` don't exactly work well in our case.
        // Marking buttons `wrap_content` always narrows down button area, producing inconsistent
        // sizing, and asking for `match_parent` results in text wrapping, as the parent is unable
        // to determine the minimum child size accurately.
        mPopupWindow.setDesiredContentSize(
                context.getResources().getDimensionPixelSize(R.dimen.fusebox_popup_width), 0);
        mPopupWindow.setHorizontalOverlapAnchor(true);
        mTabButton = mContentView.findViewById(R.id.fusebox_pick_tabs_button);
        if (ChromeFeatureList.sChromeItemPickerUi.isEnabled()) {
            mTabButton.setVisibility(View.VISIBLE);
        }
        mCameraButton = mContentView.findViewById(R.id.fusebox_camera_button);
        mGalleryButton = mContentView.findViewById(R.id.fusebox_pick_picture_button);
        mFileButton = mContentView.findViewById(R.id.fusebox_pick_file_button);
        mClipboardButton = mContentView.findViewById(R.id.fusebox_paste_from_clipboard_button);
        mAiModeButton = mContentView.findViewById(R.id.fusebox_ai_mode_button);
        mAutocompleteRequestTypeGroup =
                mContentView.findViewById(R.id.autocomplete_request_type_group);
        mAddCurrentTab = mContentView.findViewById(R.id.fusebox_add_current_tab);
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
