// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.ToolbarChildButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the download button on the toolbar. Owns the DownloadButtonView. */
@NullMarked
public class DownloadButtonCoordinator extends ToolbarChildButton {
    private final PropertyModel mModel;
    private final DownloadButtonMediator mMediator;
    private @Nullable ViewStub mViewStub;
    private @Nullable DownloadButtonView mView;
    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final Runnable mOnVisibilityChangedRunnable;
    private final NonNullObservableSupplier<Boolean> mShouldShowSupplier;
    private final Callback<Boolean> mShouldShowObserver = this::setShouldShow;

    /**
     * Creates a new {@link DownloadButtonCoordinator}.
     *
     * @param context The Android context.
     * @param viewStub The {@link ViewStub} to inflate the download button view from.
     * @param themeColorProvider The provider for theme colors.
     * @param incognitoStateProvider The provider for incognito state.
     * @param onButtonClickedRunnable Runnable invoked when the download button is clicked.
     * @param onVisibilityChangedRunnable Runnable invoked when button visibility changes.
     * @param shouldShowSupplier Supplies whether the button should be shown based on download
     *     state. Changes are forwarded to {@link #setShouldShow(boolean)}.
     */
    public DownloadButtonCoordinator(
            Context context,
            ViewStub viewStub,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            Runnable onButtonClickedRunnable,
            Runnable onVisibilityChangedRunnable,
            NonNullObservableSupplier<Boolean> shouldShowSupplier) {
        super(context, themeColorProvider, incognitoStateProvider);
        mViewStub = viewStub;
        mOnVisibilityChangedRunnable = onVisibilityChangedRunnable;
        mModel =
                new PropertyModel.Builder(DownloadButtonProperties.ALL_KEYS)
                        .with(
                                DownloadButtonProperties.TINT,
                                themeColorProvider.getActivityFocusTint())
                        .with(
                                DownloadButtonProperties.IS_INCOGNITO,
                                incognitoStateProvider.isIncognitoSelected())
                        .build();
        mMediator = new DownloadButtonMediator(mModel, onButtonClickedRunnable);
        mShouldShowSupplier = shouldShowSupplier;
        mShouldShowSupplier.addSyncObserverAndCall(mShouldShowObserver);
    }

    @Override
    public void destroy() {
        mShouldShowSupplier.removeObserver(mShouldShowObserver);
        super.destroy();
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
    }

    /**
     * Sets whether the button should be displayed.
     *
     * @param shouldShow True if the button should be displayed; false otherwise.
     */
    public void setShouldShow(boolean shouldShow) {
        if (mMediator.shouldShow() != shouldShow) {
            mMediator.setShouldShow(shouldShow);
            inflateViewIfNeeded();
            mOnVisibilityChangedRunnable.run();
        }
    }

    /**
     * Returns whether the button should be displayed.
     *
     * @return True if the button should be displayed; false otherwise.
     */
    public boolean shouldShow() {
        return mMediator.shouldShow();
    }

    // ToolbarChildButton / ToolbarWidthConsumer implementation:

    @Override
    public boolean isVisible() {
        return mMediator.isVisible();
    }

    @Override
    public boolean hasSpaceToShow() {
        return mMediator.hasSpaceToShow();
    }

    @Override
    public void setHasSpaceToShow(boolean hasSpaceToShow) {
        mMediator.setHasSpaceToShow(hasSpaceToShow);
        inflateViewIfNeeded();
    }

    @Override
    public int updateVisibility(int availableWidth) {
        // If the button is not active, consume 0 width so toolbar space is not reserved.
        if (!shouldShow()) {
            setHasSpaceToShow(false);
            return 0;
        }
        int consumedWidth = super.updateVisibility(availableWidth);
        return isVisible() ? consumedWidth : 0;
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        super.onTintChanged(tint, activityFocusTint, brandedColorScheme);
        // Use activityFocusTint so the icon dims when Chrome loses window focus (e.g.
        // multi-window).
        if (mModel != null && activityFocusTint != null) {
            mModel.set(DownloadButtonProperties.TINT, activityFocusTint);
        }
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        super.onIncognitoStateChanged(isIncognito);
        if (mModel != null) {
            mModel.set(DownloadButtonProperties.IS_INCOGNITO, isIncognito);
        }
    }

    private void inflateViewIfNeeded() {
        if (mView == null && mViewStub != null && isVisible()) {
            mView = (DownloadButtonView) mViewStub.inflate();
            mViewStub = null;
            mPropertyModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel, mView, DownloadButtonViewBinder::bind);
        }
    }

    @Nullable View getViewForTesting() {
        return mView;
    }
}
