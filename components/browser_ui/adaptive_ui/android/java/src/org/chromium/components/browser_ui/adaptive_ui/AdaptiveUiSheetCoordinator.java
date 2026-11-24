// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ScrollView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.Delegate;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.SecondaryButtonBehavior;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
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
final class AdaptiveUiSheetCoordinator
        implements AdaptiveUiControllerImpl.Presenter, BottomSheetContent {
    // Matches the bottom sheet's default half height ratio, which isn't public.
    private static final float DEFAULT_HALF_HEIGHT_RATIO = 0.75f;

    private final PropertyModel mModel;
    private final @AdaptiveUiScope int mScope;
    private final BottomSheetController mBottomSheetController;
    private final Runnable mOnShown;
    private final Callback<Integer> mOnDismissed;
    private final View mContentView;
    // The sheet's own scroll view around the description, custom view and footer. Null when the
    // custom view scrolls itself: the body is then laid out without it.
    private final @Nullable ScrollView mBodyScrollView;
    private final ViewGroup mCustomViewContainer;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mChangeProcessor;
    private final BottomSheetObserver mSheetObserver =
            new BottomSheetObserver() {
                @Override
                public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                    if (newContent != AdaptiveUiSheetCoordinator.this) {
                        mTapProtectionStartTimestampMs = -1;
                        return;
                    }
                    // The controller swaps content only while hidden, so the clock normally
                    // starts on the settled state below. If this content ever becomes current in
                    // an already-open sheet, start now so the buttons can't stay blocked.
                    mTapProtectionStartTimestampMs =
                            isSettledOpenState(mBottomSheetController.getSheetState())
                                    ? TimeUtils.elapsedRealtimeMillis()
                                    : -1;
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
                    if (mBottomSheetController.getCurrentSheetContent()
                            != AdaptiveUiSheetCoordinator.this) {
                        return;
                    }
                    // With BottomSheetDeferContentSwapOnHidden on, the hidden content is still
                    // current while observers are notified, before the BottomSheetController
                    // decides whether to destroy it.
                    if (newState == SheetState.HIDDEN) {
                        onSheetHidden(reason);
                    } else if (isSettledOpenState(newState) && mTapProtectionStartTimestampMs < 0) {
                        // Buttons sit at the bottom and are only fully visible once settled.
                        mTapProtectionStartTimestampMs = TimeUtils.elapsedRealtimeMillis();
                    }
                }
            };
    private boolean mDestroyed;
    private boolean mShown;
    private @Nullable @DismissReason Integer mRequestedDismissReason;
    private @StateChangeReason int mLastHiddenReason = StateChangeReason.NONE;
    private long mTapProtectionStartTimestampMs = -1;

    /**
     * @param context The {@link Context} used to inflate the sheet layout.
     * @param model The feature's model, built with {@link AdaptiveUiProperties#ALL_KEYS}. Its
     *     sheet-only keys ({@link AdaptiveUiProperties#SHEET_INITIAL_STATE}, {@link
     *     AdaptiveUiProperties#SHEET_USER_INITIATED} and {@link
     *     AdaptiveUiProperties#SHEET_USER_CRITICAL}) are read here.
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
        // Inflating without a root drops the layout params of the XML root, and the sheet would
        // then add this view as MATCH_PARENT. On phones a WRAP_CONTENT sheet lays its container
        // out at the full container height and only shows the content's wrapped height, so a
        // MATCH_PARENT view would push the button bar below the screen. A half sheet uses
        // RESIZE_CONTENT, which sizes the container to the sheet, so its view fills it.
        mContentView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        isHalfSheet()
                                ? ViewGroup.LayoutParams.MATCH_PARENT
                                : ViewGroup.LayoutParams.WRAP_CONTENT));
        ViewStub bodyStub = mContentView.findViewById(R.id.adaptive_ui_sheet_body_stub);
        bodyStub.setLayoutResource(
                model.get(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF)
                        ? R.layout.adaptive_ui_sheet_body_self_scrolling
                        : R.layout.adaptive_ui_sheet_body);
        bodyStub.inflate();
        mBodyScrollView = mContentView.findViewById(R.id.adaptive_ui_sheet_scroll_view);
        if (mBodyScrollView != null) {
            // A Tab stop only when it can scroll, like ModalDialogView's title scroll view.
            mBodyScrollView.addOnLayoutChangeListener(
                    (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                        boolean isScrollable =
                                v.canScrollVertically(-1) || v.canScrollVertically(1);
                        v.setFocusable(isScrollable);
                    });
        }
        mCustomViewContainer =
                mContentView.findViewById(R.id.adaptive_ui_sheet_custom_view_container);

        mContentView
                .findViewById(R.id.adaptive_ui_sheet_primary_button)
                .setOnClickListener(v -> onPrimaryButtonClicked());
        mContentView
                .findViewById(R.id.adaptive_ui_sheet_secondary_button)
                .setOnClickListener(v -> onSecondaryButtonClicked());
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
     * Hands the sheet to the {@link BottomSheetController}, which may queue it.
     *
     * @return {@code true} if the sheet is showing or taking over now; {@code false} if it was
     *     queued, suppressed, or dropped.
     */
    @Override
    public boolean show() {
        if (mDestroyed) return false;
        mBottomSheetController.addObserver(mSheetObserver);
        return mBottomSheetController.requestShowContent(this, /* animate= */ true);
    }

    /**
     * Hides the sheet if it is showing or queued; {@code onDismissed} reports {@code reason}.
     *
     * @param reason Why the entry is closed.
     * @param animate Whether to animate. When false, {@code onDismissed} fires before this returns.
     */
    @Override
    public void hide(@DismissReason int reason, boolean animate) {
        if (mDestroyed) return;
        setDismissReason(reason);
        boolean wasCurrent = isCurrent();
        mBottomSheetController.hideContent(this, animate, toStateChangeReason(reason));
        // The BottomSheetController does not destroy content it removes from its queue, so a
        // queued sheet is destroyed here. A current sheet hidden without animation is destroyed by
        // the controller synchronously inside hideContent(); destroying it here as well is a no-op
        // and only makes onDismissed() timing explicit. In both cases the reason was latched above,
        // so it does not matter that this sheet's observer no longer sees the HIDDEN change.
        if (!wasCurrent || !animate) destroy();
    }

    @Override
    public boolean isCurrent() {
        return mBottomSheetController.getCurrentSheetContent() == this;
    }

    /**
     * Records why the sheet was hidden. Does not dismiss the entry: the sheet may be suppressed or
     * put back in the queue and shown again. A later call overwrites the earlier reason.
     *
     * @param stateChangeReason The {@link StateChangeReason} of the latest HIDDEN state change.
     */
    void onSheetHidden(@StateChangeReason int stateChangeReason) {
        if (mDestroyed) return;
        mTapProtectionStartTimestampMs = -1;
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
        if (mBodyScrollView != null) return mBodyScrollView.getScrollY();
        // BottomSheetCoordinator#isContentScrolledToTop only checks `<= 0` (so a downward swipe
        // scrolls inner content up before dragging the sheet), so returning 1 or 0 suffices.
        View customView = mModel.get(AdaptiveUiProperties.CUSTOM_VIEW);
        return customView != null && canScrollUp(customView) ? 1 : 0;
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
        return !mModel.get(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED);
    }

    @Override
    public boolean showHandlebar() {
        return mModel.get(AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR);
    }

    @Override
    public float getHalfHeightRatio() {
        // The BottomSheetController opens the sheet in its lowest enabled state, so enabling the
        // half state makes the sheet open at the half height.
        if (!isHalfSheet()) return HeightMode.DISABLED;
        float halfRatio =
                getCustomRatio(AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO, HeightMode.DEFAULT);
        // A half height above the full height would open the sheet above its full height.
        assert (halfRatio == HeightMode.DEFAULT ? DEFAULT_HALF_HEIGHT_RATIO : halfRatio)
                        <= getMaxResizeContentHeightRatio()
                : "The half height ratio must not be above the full height ratio.";
        return halfRatio;
    }

    @Override
    public float getFullHeightRatio() {
        // A WRAP_CONTENT full height disables the half state, so a half sheet resizes its content
        // to the sheet height instead, which keeps the button bar on screen at the half height.
        return isHalfSheet() ? HeightMode.RESIZE_CONTENT : HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getMaxResizeContentHeightRatio() {
        // With RESIZE_CONTENT, this caps the full height of a half sheet.
        return getCustomRatio(AdaptiveUiProperties.SHEET_FULL_HEIGHT_RATIO, MAX_HEIGHT_RATIO);
    }

    @Override
    public BottomSheetType getSheetType() {
        return new BottomSheetType.Builder()
                .setModal(/* isModal= */ true)
                .setPersistent(mScope == AdaptiveUiScope.WINDOW)
                .setUserInitiated(mModel.get(AdaptiveUiProperties.SHEET_USER_INITIATED))
                .setUserCritical(mModel.get(AdaptiveUiProperties.SHEET_USER_CRITICAL))
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
        return getAccessibilityStringId(AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID);
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return getAccessibilityStringId(AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID);
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return getAccessibilityStringId(AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID);
    }

    /** Returns the ratio set for {@code key}, or {@code defaultRatio} if it's unset. */
    private float getCustomRatio(ReadableFloatPropertyKey key, float defaultRatio) {
        float ratio = mModel.get(key);
        assert ratio >= 0f && ratio <= MAX_HEIGHT_RATIO : "Invalid sheet height ratio: " + ratio;
        return ratio > 0f ? ratio : defaultRatio;
    }

    /** Returns the string ID set for {@code key}, or the generic description if it's unset. */
    private @StringRes int getAccessibilityStringId(ReadableIntPropertyKey key) {
        int id = mModel.get(key);
        return id != 0 ? id : R.string.bottom_sheet_accessibility_description;
    }

    private void onPrimaryButtonClicked() {
        if (isDismissing() || isWithinButtonTapProtectionPeriod()) return;
        Delegate delegate = mModel.get(AdaptiveUiProperties.DELEGATE);
        if (delegate != null) delegate.onPrimaryButtonClicked();
        if (mModel.get(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN)) return;
        requestDismiss(DismissReason.PRIMARY_ACTION);
    }

    private void onSecondaryButtonClicked() {
        if (isDismissing() || isWithinButtonTapProtectionPeriod()) return;
        switch (mModel.get(AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR)) {
            case SecondaryButtonBehavior.CLOSE:
                requestDismiss(DismissReason.SECONDARY_ACTION);
                return;
            case SecondaryButtonBehavior.ACTION_THEN_STAY_OPEN:
                runSecondaryButtonCallback();
                return;
            case SecondaryButtonBehavior.ACTION_THEN_DISMISS:
            default:
                runSecondaryButtonCallback();
                requestDismiss(DismissReason.SECONDARY_ACTION);
                return;
        }
    }

    private void runSecondaryButtonCallback() {
        Delegate delegate = mModel.get(AdaptiveUiProperties.DELEGATE);
        if (delegate != null) delegate.onSecondaryButtonClicked();
    }

    private boolean isWithinButtonTapProtectionPeriod() {
        long durationMs = mModel.get(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS);
        if (durationMs <= 0) return false;
        if (mTapProtectionStartTimestampMs < 0) return true;
        long now = TimeUtils.elapsedRealtimeMillis();
        boolean blocked = now <= mTapProtectionStartTimestampMs + durationMs;
        mTapProtectionStartTimestampMs = now;
        return blocked;
    }

    /**
     * Whether {@code view} or a visible descendant is scrolled down. Uses {@link
     * View#canScrollVertically}({@code -1}) because {@code RecyclerView#getScrollY()} is always 0
     * and {@code View#computeVerticalScrollOffset()} is protected.
     */
    private static boolean canScrollUp(View view) {
        if (view.getVisibility() != View.VISIBLE) return false;
        if (view.canScrollVertically(-1)) return true;
        if (!(view instanceof ViewGroup group)) return false;
        for (int i = 0; i < group.getChildCount(); i++) {
            if (canScrollUp(group.getChildAt(i))) return true;
        }
        return false;
    }

    /**
     * Whether the sheet opens at the half height. A half sheet takes its height from a ratio of the
     * screen and resizes its content to fit; any other sheet wraps its content.
     */
    private boolean isHalfSheet() {
        return mModel.get(AdaptiveUiProperties.SHEET_INITIAL_STATE) == SheetState.HALF;
    }

    private static boolean isSettledOpenState(@SheetState int state) {
        return state == SheetState.HALF || state == SheetState.FULL;
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
            case DismissReason.DISMISSED_BY_FEATURE:
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
