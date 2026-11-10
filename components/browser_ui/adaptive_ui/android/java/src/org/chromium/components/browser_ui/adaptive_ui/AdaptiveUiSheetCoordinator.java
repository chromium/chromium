// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Presents an {@link AdaptiveUiProperties} {@link PropertyModel} as a {@link BottomSheetContent}.
 *
 * <p>Inflates {@code R.layout.adaptive_ui_sheet} and binds visual properties through {@link
 * PropertyModelChangeProcessor} and {@link AdaptiveUiSheetViewBinder}. Action clicks and back
 * presses are evaluated at event time and forwarded to the dismiss request handler; the presenter
 * remains responsible for hiding the sheet through {@code BottomSheetController}.
 */
@NullMarked
final class AdaptiveUiSheetCoordinator implements BottomSheetContent {
    private final PropertyModel mModel;
    private final @AdaptiveUiScope int mScope;
    private final Callback<Integer> mDismissRequestHandler;
    private final Callback<Integer> mOnSheetDismissed;
    private final View mContentView;
    private final ScrollView mScrollView;
    private final ViewGroup mCustomViewContainer;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mChangeProcessor;
    private boolean mDismissRequested;
    private boolean mDestroyed;
    private @Nullable @DismissReason Integer mRequestedDismissReason;
    private @StateChangeReason int mLastHiddenReason = StateChangeReason.NONE;

    /**
     * @param context The {@link Context} used to inflate the sheet layout.
     * @param model The feature's model, built with {@link AdaptiveUiProperties#ALL_KEYS}.
     * @param scope The {@link AdaptiveUiScope} governing whether the sheet survives tab switches
     *     and navigation ({@link AdaptiveUiScope#WINDOW}) or is cleared automatically ({@link
     *     AdaptiveUiScope#TRANSIENT}).
     * @param dismissRequestHandler Receives the {@link DismissReason} when an interaction inside
     *     the sheet should close the entry.
     * @param onSheetDismissed Receives the {@link DismissReason} exactly once, from {@link
     *     #destroy()}, when the sheet is permanently gone. Prefers any latched reason from {@link
     *     #requestDismiss} or {@link #setDismissReason}, otherwise translates the last {@link
     *     StateChangeReason} recorded by {@link #onSheetHidden} via {@link #fromStateChangeReason}.
     */
    AdaptiveUiSheetCoordinator(
            Context context,
            PropertyModel model,
            @AdaptiveUiScope int scope,
            Callback<Integer> dismissRequestHandler,
            Callback<Integer> onSheetDismissed) {
        mOnSheetDismissed = onSheetDismissed;
        mModel = model;
        mScope = scope;
        mDismissRequestHandler = dismissRequestHandler;
        // TODO(crbug.com/580967946): Confirm sheet layout and dimensions with UX.
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.adaptive_ui_sheet, /* root= */ null);
        mScrollView = mContentView.findViewById(R.id.adaptive_ui_sheet_scroll_view);
        mCustomViewContainer =
                mContentView.findViewById(R.id.adaptive_ui_sheet_custom_view_container);

        mContentView
                .findViewById(R.id.adaptive_ui_sheet_primary_button)
                .setOnClickListener(
                        v ->
                                onActionButtonClicked(
                                        AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                        AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                                        DismissReason.PRIMARY_ACTION));
        mContentView
                .findViewById(R.id.adaptive_ui_sheet_secondary_button)
                .setOnClickListener(
                        v ->
                                onActionButtonClicked(
                                        AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                                        AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN,
                                        DismissReason.SECONDARY_ACTION));
        mContentView
                .findViewById(R.id.adaptive_ui_sheet_close_button)
                .setOnClickListener(v -> requestDismiss(DismissReason.CLOSE_BUTTON));

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mContentView, AdaptiveUiSheetViewBinder::bind);
    }

    /**
     * Latches the {@link DismissReason} to report when the sheet is subsequently destroyed. First
     * call wins: ignored if a reason was already latched (by an earlier call or an in-sheet action)
     * or after {@link #destroy()}.
     */
    void setDismissReason(@DismissReason int reason) {
        if (!mDestroyed && mRequestedDismissReason == null) {
            mRequestedDismissReason = reason;
        }
    }

    /**
     * Records why the sheet was hidden. Does not dismiss the entry: the sheet may be suppressed or
     * put back in the queue and shown again. A later call overwrites the earlier reason.
     *
     * @param stateChangeReason The {@link StateChangeReason} of the latest HIDDEN state change.
     */
    void onSheetHidden(@StateChangeReason int stateChangeReason) {
        if (mDestroyed) return;
        mLastHiddenReason = stateChangeReason;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    /**
     * Called by the {@code BottomSheetController} once the content is gone for good (hidden and not
     * put back in the queue, or dropped from the queue). Reports the dismissal exactly once.
     */
    @Override
    public void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        mChangeProcessor.destroy();
        mCustomViewContainer.removeAllViews();
        mOnSheetDismissed.onResult(
                mRequestedDismissReason != null
                        ? mRequestedDismissReason
                        : fromStateChangeReason(mLastHiddenReason));
    }

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public BottomSheetType getSheetType() {
        return new BottomSheetType.Builder()
                .setModal(/* isModal= */ true)
                .setPersistent(mScope == AdaptiveUiScope.WINDOW)
                .build();
    }

    @Override
    public boolean hasCustomLifecycle() {
        return mScope == AdaptiveUiScope.WINDOW;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
        return ObservableSuppliers.alwaysTrue();
    }

    @Override
    public void onBackPressed() {
        if (mDestroyed || mDismissRequested) return;
        OnBackPressedCallback handler = mModel.get(AdaptiveUiProperties.BACK_PRESS_HANDLER);
        if (handler != null && handler.isEnabled()) {
            handler.handleOnBackPressed();
            return;
        }
        requestDismiss(DismissReason.BACK_PRESS);
    }

    @Override
    public boolean handleBackPress() {
        onBackPressed();
        return true;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.bottom_sheet_accessibility_description;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.bottom_sheet_accessibility_description;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.bottom_sheet_accessibility_description;
    }

    private void onActionButtonClicked(
            ReadableObjectPropertyKey<@Nullable Runnable> callbackKey,
            ReadableBooleanPropertyKey keepsOpenKey,
            @DismissReason int reason) {
        if (mDestroyed || mDismissRequested) return;
        Runnable callback = mModel.get(callbackKey);
        if (callback != null) callback.run();
        if (mModel.get(keepsOpenKey)) return;
        requestDismiss(reason);
    }

    private void requestDismiss(@DismissReason int reason) {
        if (mDestroyed || mDismissRequested) return;
        mRequestedDismissReason = reason;
        mDismissRequested = true;
        mDismissRequestHandler.onResult(reason);
    }

    // LINT.IfChange(SheetDismissReasonMapping)
    static @StateChangeReason int toStateChangeReason(@DismissReason int reason) {
        switch (reason) {
            case DismissReason.BACK_PRESS:
                return StateChangeReason.BACK_PRESS;
            case DismissReason.TOUCH_OUTSIDE:
                return StateChangeReason.TAP_SCRIM;
            case DismissReason.SWIPE_DOWN:
                return StateChangeReason.SWIPE;
            case DismissReason.CLOSE_BUTTON:
                return StateChangeReason.CLOSE_BUTTON;
            case DismissReason.PRIMARY_ACTION:
            case DismissReason.SECONDARY_ACTION:
                return StateChangeReason.INTERACTION_COMPLETE;
            case DismissReason.NAVIGATION:
            case DismissReason.TAB_SWITCHED:
            case DismissReason.TAB_CLOSED:
            default:
                return StateChangeReason.NONE;
        }
    }

    private static @DismissReason int fromStateChangeReason(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS:
                return DismissReason.BACK_PRESS;
            case StateChangeReason.TAP_SCRIM:
                return DismissReason.TOUCH_OUTSIDE;
            case StateChangeReason.SWIPE:
                return DismissReason.SWIPE_DOWN;
            case StateChangeReason.CLOSE_BUTTON:
                return DismissReason.CLOSE_BUTTON;
            case StateChangeReason.INTERACTION_COMPLETE:
                return DismissReason.PRIMARY_ACTION;
            // BottomSheetManager.clearRequestsAndHide() hides with StateChangeReason.NONE for tab
            // switch, page load, and tab destroy, so a sheet closed by a tab switch reports
            // NAVIGATION while a dialog reports TAB_SWITCHED.
            case StateChangeReason.NAVIGATION:
            case StateChangeReason.NONE:
            case StateChangeReason.COMPOSITED_UI:
            case StateChangeReason.VR:
            case StateChangeReason.PROMOTE_TAB:
            case StateChangeReason.OMNIBOX_FOCUS:
            default:
                return DismissReason.NAVIGATION;
        }
    }
    // LINT.ThenChange(//components/browser_ui/adaptive_ui/android/java/src/org/chromium/components/browser_ui/adaptive_ui/DismissReason.java:DismissReason)
}
