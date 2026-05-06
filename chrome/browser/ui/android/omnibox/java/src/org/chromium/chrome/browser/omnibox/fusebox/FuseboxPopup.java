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

import java.util.HashSet;
import java.util.List;
import java.util.Set;

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

    /* package */ final View mModelsDivider;
    /* package */ final TextView mModelsHeader;
    /* package */ final List<View> mAttachmentButtons;
    /* package */ final Set<View> mDynamicThemedButtons = new HashSet<>();
    /* package */ final List<View> mDividers;
    /* package */ final List<TextView> mHeaders;

    private final DynamicRectProvider mDynamicRectProvider;
    private @PopupState int mCurrentState = PopupState.HIDDEN;

    FuseboxPopup(
            Context context,
            AnchoredPopupWindow popupWindow,
            View contentView,
            DynamicRectProvider dynamicRectProvider,
            boolean isBottomSheet) {
        mPopupWindow = popupWindow;
        mDynamicRectProvider = dynamicRectProvider;
        mViewGroup = contentView.findViewById(R.id.fusebox_view_group);
        mViewGroup.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    if (bottom - top != oldBottom - oldTop) {
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    updateLayout();
                                });
                    }
                });

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

        mModelsDivider = contentView.findViewById(R.id.fusebox_models_divider);
        mModelsHeader = contentView.findViewById(R.id.fusebox_models_header);
        mAttachmentButtons =
                List.of(
                        mAddCurrentTab,
                        mClipboardButton,
                        mTabButton,
                        mCameraButton,
                        mGalleryButton,
                        mFileButton);
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
        mCurrentState = state;
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

        updateLayout();
        show();
    }

    /**
     * Update the layout of the popup if it is showing. This is useful when contents change
     * visibility.
     */
    void updateLayout() {
        if (!isShowing() || mCurrentState == PopupState.HIDDEN) return;
        int width = mDynamicRectProvider.getPopupWidth(mCurrentState, mViewGroup.getResources());
        mPopupWindow.updateDesiredContentSize(width, /* height= */ 0, /* updateLayout= */ true);
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
