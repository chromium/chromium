// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.PopupWindow;

import androidx.core.view.accessibility.AccessibilityEventCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.function.Supplier;

/**
 * Coordinator for the page zoom indicator. This class is responsible for creating and managing the
 * page zoom indicator view.
 */
@NullMarked
public class PageZoomIndicatorCoordinator {
    private final PageZoomManager mManager;
    private final PageZoomIndicatorMediator mMediator;
    private final Supplier<@Nullable View> mZoomIndicatorViewSupplier;

    private @Nullable ZoomEventsObserver mZoomEventsObserver;
    private @Nullable Runnable mOnDismissCallback;
    private @Nullable Callback<Double> mOnZoomLevelChangedCallback;
    private @Nullable PopupWindow mPopupWindow;
    private @Nullable View mView;

    /**
     * @param zoomIndicatorViewSupplier Supplier of the view to anchor the indicator to.
     * @param manager The manager used to interact with the zoom functionality.
     */
    public PageZoomIndicatorCoordinator(
            Supplier<@Nullable View> zoomIndicatorViewSupplier, PageZoomManager manager) {
        mZoomIndicatorViewSupplier = zoomIndicatorViewSupplier;
        mManager = manager;
        mMediator = new PageZoomIndicatorMediator(mManager);

        mZoomEventsObserver =
                new ZoomEventsObserver() {
                    @Override
                    public void onZoomLevelChanged(String host, double newZoomLevel) {
                        if (mOnZoomLevelChangedCallback != null) {
                            mOnZoomLevelChangedCallback.onResult(newZoomLevel);
                        }
                    }
                };
    }

    /**
     * Called when the native library is initialized. Since the zoom events observer needs to be
     * scoped by profile, we need to wait until the native library is initialized to add the
     * observer.
     */
    public void onNativeInitialized() {
        assert mZoomEventsObserver != null;
        mManager.addZoomEventsObserver(mZoomEventsObserver);
        // Set initial zoom level.
        if (mOnZoomLevelChangedCallback != null) {
            mOnZoomLevelChangedCallback.onResult(mManager.getZoomLevel());
        }
    }

    /**
     * @param onDismissCallback The callback to be invoked.
     */
    public void setOnDismissCallbacks(@Nullable Runnable onDismissCallback) {
        mOnDismissCallback = onDismissCallback;
    }

    /**
     * @param onZoomLevelChangedCallback The callback to be invoked when the zoom level changes. The
     *     double parameter is the new zoom level.
     */
    public void setOnZoomLevelChangedCallback(
            @Nullable Callback<Double> onZoomLevelChangedCallback) {
        mOnZoomLevelChangedCallback = onZoomLevelChangedCallback;
    }

    /**
     * Show the zoom feature UI to the user.
     *
     * @param webContents WebContents that this zoom UI will control.
     */
    public void show(WebContents webContents) {
        // This cannot be null, since this is called after the zoom button is clicked.
        assumeNonNull(mZoomIndicatorViewSupplier.get());
        if (mPopupWindow == null) {
            mView =
                    LayoutInflater.from(mZoomIndicatorViewSupplier.get().getContext())
                            .inflate(R.layout.page_zoom_indicator_view, null);

            mPopupWindow = mMediator.buildPopupWindow(mView, this::hide);
        }
        if (mPopupWindow.isShowing()) return;

        mMediator.pushProperties();
        assumeNonNull(mView);

        // Post the accessibility event to ensure the view is fully ready to receive focus before
        // the announcement is made. This prevents a race condition where TalkBack might focus on an
        // intermediate, unlabeled view.
        mView.post(() -> sendPaneChangeAccessibilityEvent(/* isShowing= */ true));
        mMediator.showPopupWindow(mZoomIndicatorViewSupplier.get(), mPopupWindow);

        PageZoomUma.logZoomIndicatorClicked();
    }

    /** Hide the zoom feature UI from the user. */
    public void hide() {
        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
            sendPaneChangeAccessibilityEvent(/* isShowing= */ false);
        }
        if (mOnDismissCallback != null) mOnDismissCallback.run();
    }

    /** Clean-up views and children during destruction. */
    public void destroy() {
        hide();
        if (mPopupWindow != null) {
            mPopupWindow.setOnDismissListener(null);
            mPopupWindow = null;
        }
        mView = null;
        mOnZoomLevelChangedCallback = null;
        mOnDismissCallback = null;

        if (mZoomEventsObserver != null) {
            mManager.removeZoomEventsObserver(mZoomEventsObserver);
            mZoomEventsObserver = null;
        }
    }

    /**
     * Returns true if the given zoom level is the default zoom level for the current Profile.
     *
     * @return True if the given zoom level is the default zoom level for the current Profile, false
     *     otherwise.
     */
    public boolean isZoomLevelDefault() {
        if (mMediator.isCurrentTabNull()) return true;
        return mMediator.isZoomLevelDefault();
    }

    /** Returns true if the popup window is showing. */
    public boolean isPopupWindowShowing() {
        return mPopupWindow != null && mPopupWindow.isShowing();
    }

    /**
     * Sends accessibility events for pane appearance/disappearance when the message is shown/hidden
     * respectively. This should ideally move accessibility focus automatically to/out of the
     * message view as applicable.
     *
     * @param isShowing Whether the message is visible. {@code true} if shown, {@code false} if
     *     hidden.
     */
    @SuppressWarnings("WrongConstant")
    private void sendPaneChangeAccessibilityEvent(boolean isShowing) {
        AccessibilityEvent event =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        if (isShowing) {
            event.setContentChangeTypes(AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_APPEARED);
        } else {
            event.setContentChangeTypes(
                    AccessibilityEventCompat.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED);
        }
        AccessibilityState.sendAccessibilityEvent(event);
    }
}
