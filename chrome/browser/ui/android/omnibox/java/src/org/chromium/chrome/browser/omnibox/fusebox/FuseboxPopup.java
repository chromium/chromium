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

import java.util.List;

/** A popup for the Fusebox component. */
@NullMarked
class FuseboxPopup {
    /* package */ final AnchoredPopupWindow mPopupWindow;
    /* package */ final Button mAddCurrentTab;
    /* package */ final Button mTabButton;
    /* package */ final Button mCameraButton;
    /* package */ final Button mGalleryButton;
    /* package */ final Button mFileButton;
    /* package */ final Button mClipboardButton;
    /* package */ final Button mAiModeButton;
    /* package */ final Button mCreateImageButton;
    /* package */ final View mRequestTypeDivider;

    /* package */ final List<Button> mButtons;
    /* package */ final List<View> mDividers;

    FuseboxPopup(Context context, AnchoredPopupWindow popupWindow, View contentView) {
        mPopupWindow = popupWindow;
        // `match_parent` and `wrap_content` don't exactly work well in our case.
        // Marking buttons `wrap_content` always narrows down button area, producing inconsistent
        // sizing, and asking for `match_parent` results in text wrapping, as the parent is unable
        // to determine the minimum child size accurately.
        mPopupWindow.setDesiredContentSize(
                context.getResources().getDimensionPixelSize(R.dimen.fusebox_popup_width), 0);
        mPopupWindow.setHorizontalOverlapAnchor(true);
        mPopupWindow.setVerticalOverlapAnchor(true);
        mTabButton = contentView.findViewById(R.id.fusebox_pick_tabs_button);
        if (ChromeFeatureList.sChromeItemPickerUi.isEnabled()) {
            mTabButton.setVisibility(View.VISIBLE);
        }
        mCameraButton = contentView.findViewById(R.id.fusebox_camera_button);
        mGalleryButton = contentView.findViewById(R.id.fusebox_pick_picture_button);
        mFileButton = contentView.findViewById(R.id.fusebox_pick_file_button);
        mClipboardButton = contentView.findViewById(R.id.fusebox_paste_from_clipboard_button);
        mAiModeButton = contentView.findViewById(R.id.fusebox_ai_mode_button);
        mCreateImageButton = contentView.findViewById(R.id.fusebox_create_image_button);
        mAddCurrentTab = contentView.findViewById(R.id.fusebox_add_current_tab);
        mRequestTypeDivider = contentView.findViewById(R.id.fusebox_request_type_divider);

        mButtons =
                List.of(
                        mAddCurrentTab,
                        mClipboardButton,
                        mTabButton,
                        mCameraButton,
                        mGalleryButton,
                        mFileButton,
                        mAiModeButton,
                        mCreateImageButton);
        mDividers = List.of(mRequestTypeDivider);
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
