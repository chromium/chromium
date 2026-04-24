// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.TextView;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupState;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.util.ArrayList;
import java.util.List;

/** A popup for the Fusebox component. */
@NullMarked
class FuseboxPopup {
    /**
     * Delay (in milliseconds) between calling up the popup window and requesting focus for
     * accessibility. This is needed because Popup views are not shown instantaneously.
     */
    private static final int ACCESSIBILITY_VIEW_FOCUS_DELAY_MS = 500;

    /* package */ final AnchoredPopupWindow mPopupWindow;
    /* package */ final ViewGroup mViewGroup;
    /* package */ final View mAddCurrentTab;
    /* package */ final View mTabButton;
    /* package */ final View mClipboardButton;
    /* package */ final View mCameraButton;
    /* package */ final View mGalleryButton;
    /* package */ final View mFileButton;
    /* package */ final View mToolsDivider;
    /* package */ final TextView mToolsHeader;
    /* package */ final View mAiModeButton;
    /* package */ final View mCreateImageButton;
    /* package */ final View mDeepSearchButton;
    /* package */ final View mCanvasButton;
    /* package */ final View mModelsDivider;
    /* package */ final TextView mModelsHeader;
    /* package */ final List<View> mButtons;
    /* package */ final List<View> mDividers;
    /* package */ final List<TextView> mHeaders;

    private final DynamicRectProvider mDynamicRectProvider;

    FuseboxPopup(
            Context context,
            AnchoredPopupWindow popupWindow,
            View contentView,
            DynamicRectProvider dynamicRectProvider,
            boolean isBottomSheet) {
        mPopupWindow = popupWindow;
        mDynamicRectProvider = dynamicRectProvider;
        mViewGroup = contentView.findViewById(R.id.fusebox_view_group);

        ViewStub stub = contentView.findViewById(R.id.fusebox_attachments_stub);
        stub.setLayoutResource(
                isBottomSheet
                        ? R.layout.fusebox_horizontal_attachments
                        : R.layout.fusebox_vertical_attachments);
        stub.inflate();

        mAddCurrentTab = contentView.findViewById(R.id.fusebox_add_current_tab);
        mTabButton = contentView.findViewById(R.id.fusebox_pick_tabs_button);
        mClipboardButton = contentView.findViewById(R.id.fusebox_paste_from_clipboard_button);
        mCameraButton = contentView.findViewById(R.id.fusebox_camera_button);
        mGalleryButton = contentView.findViewById(R.id.fusebox_pick_picture_button);
        mFileButton = contentView.findViewById(R.id.fusebox_pick_file_button);

        mToolsDivider = contentView.findViewById(R.id.fusebox_tools_divider);
        mToolsHeader = contentView.findViewById(R.id.fusebox_tools_header);
        mAiModeButton = contentView.findViewById(R.id.fusebox_ai_mode_button);
        mCreateImageButton = contentView.findViewById(R.id.fusebox_create_image_button);
        mDeepSearchButton = contentView.findViewById(R.id.fusebox_deep_search_button);
        mCanvasButton = contentView.findViewById(R.id.fusebox_canvas_button);

        initializeItem(mAddCurrentTab, R.string.fusebox_add_current_tab, 0, 0);
        initializeItem(
                mTabButton,
                R.string.omnibox_navattach_tabs,
                R.drawable.ic_features_24dp,
                R.string.accessibility_omnibox_add_tabs);
        // TODO(crbug.com/436888404): either drop clipboard or use proper strings.
        initializeItem(
                mClipboardButton,
                R.string.clipboard_permission_title,
                R.drawable.ic_content_copy,
                0);
        initializeItem(
                mCameraButton,
                R.string.photo_picker_camera,
                R.drawable.ic_photo_camera,
                R.string.accessibility_omnibox_add_camera_picture);
        initializeItem(
                mGalleryButton,
                R.string.omnibox_navattach_gallery,
                R.drawable.ic_photo_library_fill_24dp,
                R.string.accessibility_omnibox_add_images);
        initializeItem(
                mFileButton,
                R.string.omnibox_navattach_files,
                R.drawable.ic_attach_file_24dp,
                R.string.accessibility_omnibox_add_files);
        initializeItem(
                mAiModeButton,
                R.string.ai_mode_entrypoint_label,
                R.drawable.search_spark_black_24dp,
                R.string.accessibility_omnibox_enable_ai_mode);
        initializeItem(
                mCreateImageButton,
                R.string.omnibox_create_image,
                R.drawable.create_image_24dp,
                R.string.accessibility_omnibox_create_image);
        initializeItem(
                mDeepSearchButton,
                R.string.ntp_compose_deep_search,
                R.drawable.travel_explore_24dp,
                0);
        initializeItem(mCanvasButton, R.string.ntp_compose_canvas, R.drawable.draft_spark_24dp, 0);

        mModelsDivider = contentView.findViewById(R.id.fusebox_models_divider);
        mModelsHeader = contentView.findViewById(R.id.fusebox_models_header);

        mButtons =
                new ArrayList<>(
                        List.of(
                                mAddCurrentTab,
                                mClipboardButton,
                                mTabButton,
                                mCameraButton,
                                mGalleryButton,
                                mFileButton,
                                mAiModeButton,
                                mCreateImageButton,
                                mDeepSearchButton,
                                mCanvasButton));
        mDividers = List.of(mToolsDivider, mModelsDivider);
        mHeaders = List.of(mToolsHeader, mModelsHeader);
    }

    /** Show the popup window. */
    void show() {
        mPopupWindow.show();
        // TODO(crbug.com/470324794): This isn't right. Figure out why AnchoredPopupWindow won't
        // focus views for us.
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                this::focusFirstViewForAccessibility,
                ACCESSIBILITY_VIEW_FOCUS_DELAY_MS);
    }

    /**
     * Apply the requested PopupState to the popup. This may involve switching the anchor and
     * updating the content size.
     *
     * @param state The target state of the popup.
     */
    void setPopupState(@PopupState int state) {
        if (state == FuseboxProperties.PopupState.BOTTOM) {
            mPopupWindow.setAnimationStyle(R.style.FuseboxBottomSheetAnimation);
        } else {
            mPopupWindow.setAnimationStyle(0);
        }

        // ALWAYS update the DynamicRectProvider state first.
        // This ensures it correctly unregisters layout observers when transitioning to HIDDEN.
        mDynamicRectProvider.setPopupState(state);

        if (state == PopupState.HIDDEN) {
            dismiss();
            return;
        }

        int width =
                mDynamicRectProvider.getPopupWidth(state, mViewGroup.getContext().getResources());

        mPopupWindow.updateDesiredContentSize(width, /* height= */ 0, /* updateLayout= */ true);
        show();
    }

    private void initializeItem(View item, int textRes, int iconRes, int a11yRes) {
        TextView actionText = item.findViewById(R.id.action_text);
        actionText.setText(textRes);

        if (iconRes != 0) {
            ImageView actionIcon = item.findViewById(R.id.start_icon);
            actionIcon.setImageResource(iconRes);
        }

        if (a11yRes != 0) {
            item.setContentDescription(item.getContext().getString(a11yRes));
        }
    }

    /**
     * Focuses for accessibility the first view marked as important for accessibility.
     *
     * <p>This is important because Android Popup windows are not focused for accessibility by
     * default and do not automatically move the Accessibility focus when called up.
     *
     * <p>TODO(crbug.com/470324794): This isn't right. Figure out why AnchoredPopupWindow won't
     * focus views for us.
     */
    void focusFirstViewForAccessibility() {
        View viewForAccessibility = null;
        for (int viewIndex = 0; viewIndex < mViewGroup.getChildCount(); viewIndex++) {
            var view = mViewGroup.getChildAt(viewIndex);

            if (view.getVisibility() == View.VISIBLE && view.isImportantForAccessibility()) {
                viewForAccessibility = view;
                break;
            }
        }

        if (viewForAccessibility == null) return;

        // Move focus to the view, emitting event.
        viewForAccessibility.requestFocus();
        viewForAccessibility.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /** Dismiss the popup window. */
    void dismiss() {
        mPopupWindow.dismiss();
    }

    /** Returns whether the popup window is currently showing. */
    boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /** Add a listener for when the popup is dismissed. */
    void addOnDismissListener(OnDismissListener listener) {
        mPopupWindow.addOnDismissListener(listener);
    }
}
