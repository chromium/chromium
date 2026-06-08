// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.TextView;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.window.layout.WindowMetrics;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetObserver;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

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
    /* package */ final FuseboxScrollView mScrollView;
    /* package */ final View mAddCurrentTab;
    /* package */ final View mTabButton;

    /* package */ final View mCameraButton;
    /* package */ final View mGalleryButton;
    /* package */ final View mFileButton;
    /* package */ final View mToolsDivider;
    /* package */ final TextView mToolsHeader;

    /* package */ final View mModelsDivider;
    /* package */ final TextView mModelsHeader;
    /* package */ final @Nullable TextView mRecentTabsHeader;
    /* package */ final @Nullable ViewGroup mRecentTabsContainer;
    /* package */ final @Nullable View mRecentTabsDivider;
    /* package */ final List<View> mAttachmentButtons;
    /* package */ final Set<View> mDynamicThemedButtons = new HashSet<>();
    /* package */ final List<View> mDividers;
    /* package */ final List<TextView> mHeaders;

    private final Activity mActivity;
    private final DynamicRectProvider mDynamicRectProvider;
    private final @Nullable InsetObserver mInsetObserver;
    private final int mInitialScrollPaddingBottom;
    private @PopupState int mCurrentState = PopupState.HIDDEN;
    private @Nullable Integer mPreviousAccessibilityImportance;
    private @Nullable View mCachedContentView;

    private final RectProvider.Observer mDynamicRectObserver =
            new RectProvider.Observer() {
                @Override
                public void onRectChanged() {
                    updateLayout();
                }

                @Override
                public void onRectHidden() {}
            };

    private final WindowInsetObserver mWindowInsetObserver =
            new WindowInsetObserver() {
                @Override
                public void onInsetChanged() {
                    updateLayout();
                }

                @Override
                public void onKeyboardInsetChanged(int inset) {
                    // The popup covers the ime, so we don't need to do anything when it changes.
                }
            };

    FuseboxPopup(
            Activity activity,
            WindowAndroid windowAndroid,
            AnchoredPopupWindow popupWindow,
            View contentView,
            DynamicRectProvider dynamicRectProvider,
            boolean isBottomSheet) {
        mActivity = activity;
        mPopupWindow = popupWindow;
        mPopupWindow.setClippingEnabled(false);
        mPopupWindow.addOnDismissListener(this::restoreBackgroundAccessibility);
        mDynamicRectProvider = dynamicRectProvider;
        mDynamicRectProvider.startObserving(mDynamicRectObserver);
        mInsetObserver = windowAndroid.getInsetObserver();
        if (mInsetObserver != null) {
            mInsetObserver.addObserver(mWindowInsetObserver);
        }

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
        mScrollView = contentView.findViewById(R.id.fusebox_scroll_view);
        mScrollView.setLayoutDirection(
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR);
        mInitialScrollPaddingBottom = mScrollView.getPaddingBottom();

        if (isBottomSheet) {
            mScrollView.setOnSwipeDownListener(this::dismiss);
        }

        ViewStub stub = contentView.findViewById(R.id.fusebox_attachments_stub);
        stub.setLayoutResource(
                isBottomSheet
                        ? R.layout.fusebox_horizontal_attachments
                        : R.layout.fusebox_vertical_attachments);
        stub.inflate();

        mAddCurrentTab = contentView.findViewById(R.id.fusebox_add_current_tab);
        mTabButton = contentView.findViewById(R.id.fusebox_pick_tabs_button);

        mCameraButton = contentView.findViewById(R.id.fusebox_camera_button);
        mGalleryButton = contentView.findViewById(R.id.fusebox_pick_picture_button);
        mFileButton = contentView.findViewById(R.id.fusebox_pick_file_button);

        mToolsDivider = contentView.findViewById(R.id.fusebox_tools_divider);
        mToolsHeader = contentView.findViewById(R.id.fusebox_tools_header);

        initializeItem(
                mAddCurrentTab,
                isBottomSheet ? R.string.fusebox_add_tab : R.string.fusebox_add_current_tab,
                0,
                0);
        initializeItem(
                mTabButton,
                R.string.omnibox_navattach_tabs,
                R.drawable.ic_features_24dp,
                R.string.accessibility_omnibox_add_tabs);

        initializeItem(
                mCameraButton,
                R.string.photo_picker_camera,
                R.drawable.photo_camera_24dp,
                R.string.accessibility_omnibox_add_camera_picture);
        initializeItem(
                mGalleryButton,
                R.string.omnibox_navattach_gallery,
                R.drawable.photo_24dp,
                R.string.accessibility_omnibox_add_images);
        initializeItem(
                mFileButton,
                R.string.omnibox_navattach_files,
                R.drawable.ic_attach_file_24dp,
                R.string.accessibility_omnibox_add_files);

        mModelsDivider = contentView.findViewById(R.id.fusebox_models_divider);
        mModelsHeader = contentView.findViewById(R.id.fusebox_models_header);

        mRecentTabsDivider = contentView.findViewById(R.id.fusebox_recent_tabs_divider);
        mRecentTabsHeader = contentView.findViewById(R.id.fusebox_recent_tabs_header);
        mRecentTabsContainer = contentView.findViewById(R.id.fusebox_recent_tabs_container);

        mAttachmentButtons =
                List.of(mAddCurrentTab, mTabButton, mCameraButton, mGalleryButton, mFileButton);

        mDividers =
                mRecentTabsDivider != null
                        ? List.of(mRecentTabsDivider, mToolsDivider, mModelsDivider)
                        : List.of(mToolsDivider, mModelsDivider);
        mHeaders =
                mRecentTabsHeader != null
                        ? List.of(mRecentTabsHeader, mToolsHeader, mModelsHeader)
                        : List.of(mToolsHeader, mModelsHeader);
    }

    void destroy() {
        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(mWindowInsetObserver);
        }
        mScrollView.setOnSwipeDownListener(null);
        mDynamicRectProvider.stopObserving(mDynamicRectObserver);
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
        if (state == PopupState.BOTTOM) {
            mPopupWindow.setAnimationStyle(R.style.FuseboxBottomSheetAnimation);
            hideBackgroundAccessibility();
        } else {
            mPopupWindow.setAnimationStyle(0);
            restoreBackgroundAccessibility();
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

    private void hideBackgroundAccessibility() {
        if (mPreviousAccessibilityImportance != null) return;
        mCachedContentView = mActivity.findViewById(android.R.id.content);
        if (mCachedContentView != null) {
            mPreviousAccessibilityImportance = mCachedContentView.getImportantForAccessibility();
            mCachedContentView.setImportantForAccessibility(
                    View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }
    }

    private void restoreBackgroundAccessibility() {
        if (mPreviousAccessibilityImportance == null) return;
        if (mCachedContentView != null) {
            mCachedContentView.setImportantForAccessibility(mPreviousAccessibilityImportance);
            mCachedContentView = null;
        }
        mPreviousAccessibilityImportance = null;
    }

    /**
     * Update the layout of the popup if it is showing. This is useful when contents change
     * visibility.
     */
    void updateLayout() {
        if (!isShowing() || mCurrentState == PopupState.HIDDEN) return;
        updateInsets();
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
    @SuppressWarnings("AccessibilityFocus")
    void focusFirstViewForAccessibility() {
        View viewForAccessibility = findFirstViewForAccessibility(mViewGroup);
        if (viewForAccessibility == null) return;

        // Move focus to the view, emitting event.
        viewForAccessibility.requestFocus();
        viewForAccessibility.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        viewForAccessibility.performAccessibilityAction(
                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
    }

    private @Nullable View findFirstViewForAccessibility(View view) {
        if (view.getVisibility() != View.VISIBLE) return null;
        if (view.isImportantForAccessibility()) return view;

        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View result = findFirstViewForAccessibility(viewGroup.getChildAt(i));
                if (result != null) return result;
            }
        }
        return null;
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

    private boolean shouldApplyBottomInsets(WindowInsetsCompat insets) {
        if (!mActivity.isInMultiWindowMode()) return true;

        WindowMetrics windowMetrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(mActivity);
        WindowMetrics maxWindowMetrics =
                WindowMetricsCalculator.getOrCreate().computeMaximumWindowMetrics(mActivity);

        int currentBottom = windowMetrics.getBounds().bottom;
        int maxBottom = maxWindowMetrics.getBounds().bottom;

        Insets navBarInsets = insets.getInsets(WindowInsetsCompat.Type.navigationBars());

        return (maxBottom - currentBottom) <= navBarInsets.bottom;
    }

    private void updateInsets() {
        if (mInsetObserver == null || mCurrentState != PopupState.BOTTOM) return;

        WindowInsetsCompat insets = mInsetObserver.getLastRawWindowInsets();
        if (insets == null) return;

        int bottomPadding = mInitialScrollPaddingBottom;
        if (shouldApplyBottomInsets(insets)) {
            Insets navBarInsets = insets.getInsets(WindowInsetsCompat.Type.navigationBars());
            bottomPadding += navBarInsets.bottom;
        }

        mScrollView.setPaddingRelative(
                mScrollView.getPaddingStart(),
                mScrollView.getPaddingTop(),
                mScrollView.getPaddingEnd(),
                bottomPadding);
    }
}
