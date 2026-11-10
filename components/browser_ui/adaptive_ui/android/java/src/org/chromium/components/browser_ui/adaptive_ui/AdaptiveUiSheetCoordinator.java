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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
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
 * presses are evaluated at event time and hide the sheet.
 *
 * <p>Also drives the {@link BottomSheetController}: {@link #show()} requests the sheet, and the
 * owner hears when it is shown and when it is gone. A HIDDEN state change only records why the
 * sheet was hidden, because the sheet may be suppressed or put back in the queue and shown again.
 * The entry is dismissed once the {@link BottomSheetController} calls {@link #destroy()}.
 */
@NullMarked
final class AdaptiveUiSheetCoordinator implements BottomSheetContent {
    private final PropertyModel mModel;
    private final @AdaptiveUiScope int mScope;
    private final BottomSheetController mBottomSheetController;
    private final Runnable mOnShown;
    private final Callback<Integer> mOnDismissed;
    private final View mContentView;
    private final ScrollView mScrollView;
    private final ViewGroup mCustomViewContainer;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mChangeProcessor;
    private final BottomSheetObserver mSheetObserver =
            new BottomSheetObserver() {
                @Override
                public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                    if (newContent != AdaptiveUiSheetCoordinator.this) return;
                    // Shown again after a HIDDEN that was not a dismissal (suppressed, or pushed
                    // back by higher-priority content): a hide requested before that did not
                    // happen, so accept input again.
                    mRequestedDismissReason = null;
                    notifyShown();
                }

                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    // Sent while this content is still current, before the HIDDEN state change.
                    // Recording the reason here too keeps it right when the BottomSheetController
                    // destroys the content before this observer sees HIDDEN (when
                    // BottomSheetDeferContentSwapOnHidden is off).
                    if (mBottomSheetController.getCurrentSheetContent()
                            == AdaptiveUiSheetCoordinator.this) {
                        onSheetHidden(reason);
                    }
                }

                @Override
                public void onSheetStateChanged(
                        @SheetState int newState, @StateChangeReason int reason) {
                    // With BottomSheetDeferContentSwapOnHidden on, the hidden content is still
                    // current while observers are notified, before the BottomSheetController
                    // decides whether to destroy it.
                    if (newState == SheetState.HIDDEN
                            && mBottomSheetController.getCurrentSheetContent()
                                    == AdaptiveUiSheetCoordinator.this) {
                        onSheetHidden(reason);
                    }
                }
            };
    private boolean mDestroyed;
    private boolean mShown;
    private @Nullable @DismissReason Integer mRequestedDismissReason;
    private @StateChangeReason int mLastHiddenReason = StateChangeReason.NONE;

    /**
     * @param context The {@link Context} used to inflate the sheet layout.
     * @param model The feature's model, built with {@link AdaptiveUiProperties#ALL_KEYS}.
     * @param scope The {@link AdaptiveUiScope} governing whether the sheet survives tab switches
     *     and navigation ({@link AdaptiveUiScope#WINDOW}) or is cleared automatically ({@link
     *     AdaptiveUiScope#TRANSIENT}).
     * @param bottomSheetController The window's {@link BottomSheetController} that shows the sheet.
     * @param onShown Runs once, when the {@link BottomSheetController} takes this content (the
     *     sheet starts opening). Not run again if the sheet is suppressed or re-queued and later
     *     shown again.
     * @param onDismissed Receives the {@link DismissReason} exactly once, from {@link #destroy()},
     *     when the sheet is gone. Prefers any latched reason from {@link #requestDismiss} or {@link
     *     #hide}, otherwise translates the last {@link StateChangeReason} recorded by {@link
     *     #onSheetHidden} via {@link #fromStateChangeReason}.
     */
    AdaptiveUiSheetCoordinator(
            Context context,
            PropertyModel model,
            @AdaptiveUiScope int scope,
            BottomSheetController bottomSheetController,
            Runnable onShown,
            Callback<Integer> onDismissed) {
        mModel = model;
        mScope = scope;
        mBottomSheetController = bottomSheetController;
        mOnShown = onShown;
        mOnDismissed = onDismissed;
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

    /** Hands the sheet to the {@link BottomSheetController}, which may queue it. */
    void show() {
        if (mDestroyed) return;
        mBottomSheetController.addObserver(mSheetObserver);
        mBottomSheetController.requestShowContent(this, /* animate= */ true);
    }

    /**
     * Hides the sheet if it is showing or queued; {@code onDismissed} reports {@code reason}.
     *
     * @param reason Why the entry is closed.
     * @param animate Whether to animate. When false, {@code onDismissed} fires before this returns.
     */
    void hide(@DismissReason int reason, boolean animate) {
        if (mDestroyed) return;
        setDismissReason(reason);
        boolean isCurrent = mBottomSheetController.getCurrentSheetContent() == this;
        mBottomSheetController.hideContent(this, animate, toStateChangeReason(reason));
        // The BottomSheetController does not destroy content it removes from its queue, so a
        // queued sheet is destroyed here. A current sheet hidden without animation is destroyed by
        // the controller synchronously inside hideContent(); destroying it here as well is a no-op
        // and only makes onDismissed() timing explicit. In both cases the reason was latched above,
        // so it does not matter that this sheet's observer no longer sees the HIDDEN change.
        if (!isCurrent || !animate) destroy();
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
     * Called by the {@link BottomSheetController} once the content is gone for good (hidden and not
     * put back in the queue, or dropped from the queue), or by {@link #hide}. Reports the dismissal
     * exactly once.
     */
    @Override
    public void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        mBottomSheetController.removeObserver(mSheetObserver);
        mChangeProcessor.destroy();
        mCustomViewContainer.removeAllViews();
        mOnDismissed.onResult(
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
        if (isDismissing()) return;
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
        if (isDismissing()) return;
        Runnable callback = mModel.get(callbackKey);
        if (callback != null) callback.run();
        if (mModel.get(keepsOpenKey)) return;
        requestDismiss(reason);
    }

    /** Whether the sheet is destroyed or a dismissal was already requested. */
    private boolean isDismissing() {
        return mDestroyed || mRequestedDismissReason != null;
    }

    private void requestDismiss(@DismissReason int reason) {
        if (isDismissing()) return;
        mRequestedDismissReason = reason;
        hide(reason, /* animate= */ true);
    }

    private void notifyShown() {
        // A sheet put back in the queue and shown again is still the same presentation.
        if (mDestroyed || mShown) return;
        mShown = true;
        mOnShown.run();
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
