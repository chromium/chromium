// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.annotation.SuppressLint;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.EducationOnboardingEvent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.EducationOnboardingType;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.IphEvent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.IphFeature;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.browser_ui.widget.textbubble.ArrowBubbleDrawable;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/**
 * Coordinates the Bottom Bar Education IPH flow.
 *
 * <p>This flow relies on a scrim-below-bottom-controls z-order trick. When the bottom controls are
 * positioned at the bottom, we use the `control_container` (which contains the bottom omnibox) as
 * the scrim anchor view. By setting `SHOW_IN_FRONT_OF_ANCHOR_VIEW = false`, the scrim is inserted
 * before the `control_container` in the view hierarchy, leaving both the `control_container` and
 * the subsequently-drawn `bottom_app_bar_container` (containing the bottom bar) undimmed above the
 * scrim.
 *
 * <p>When the controls are positioned at the top, the `control_container` is at the top of the
 * screen. In this case, we use the `bottom_app_bar_container` directly as the anchor view, so only
 * the bottom bar remains above the scrim.
 */
@NullMarked
public class BottomBarEducationCoordinator
        implements BrowserControlsStateProvider.Observer, ComponentCallbacks {

    private static final int NO_TOOLTIP_STEP = -1;

    static final @ActionId int[] TOOLTIP_ACTION_IDS = {
        ActionId.GLIC, ActionId.NEW_TAB, ActionId.TAB_SWITCHER
    };
    private static final @StringRes int[] TOOLTIP_STRING_IDS = {
        R.string.bottom_bar_education_tooltip_gemini,
        R.string.bottom_bar_education_tooltip_new_tab,
        R.string.bottom_bar_education_tooltip_tab_switcher
    };

    private static boolean sSkipTooltipShowCheckForTesting;
    private static boolean sEducationShownThisSession;

    private final Context mContext;
    private final ScrimManager mScrimManager;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final @Nullable View mControlContainer;
    private final @Nullable View mBottomAppBarContainer;
    private final BottomBarView mBottomBarView;
    private final Supplier<@Nullable Profile> mProfileSupplier;

    private @Nullable AnchoredPopupWindow mPopupWindow;
    private @Nullable PropertyModel mScrimModel;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private ViewTreeObserver.@Nullable OnGlobalLayoutListener mGlobalLayoutListener;
    private @Nullable Tracker mActiveTracker;
    private @State int mState = State.IDLE;
    private int mLastOrientation = Configuration.ORIENTATION_UNDEFINED;
    private @Nullable TextBubble mCurrentTooltip;
    private @Nullable View mCurrentHighlightView;
    private int mTooltipStep;
    private @Nullable @EducationOnboardingType String mActiveBubbleOnboardingType;
    private int mLastShownTooltipStep = NO_TOOLTIP_STEP;

    @IntDef({State.IDLE, State.SHOWING_BUBBLE, State.SHOWING_TOOLTIPS, State.DONE})
    @Retention(RetentionPolicy.SOURCE)
    @interface State {
        int IDLE = 0;
        int SHOWING_BUBBLE = 1;
        int SHOWING_TOOLTIPS = 2;
        int DONE = 3;
    }

    /**
     * Constructs the BottomBarEducationCoordinator.
     *
     * @param scrimManager The {@link ScrimManager} used to show the dark scrim.
     * @param browserControlsVisibilityManager The manager for browser controls.
     * @param controlContainer The top control container view.
     * @param bottomAppBarContainer The bottom app bar container view.
     * @param bottomBarView The bottom bar view itself.
     * @param profileSupplier Supplier for the current Profile.
     */
    public BottomBarEducationCoordinator(
            ScrimManager scrimManager,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            @Nullable View controlContainer,
            @Nullable View bottomAppBarContainer,
            BottomBarView bottomBarView,
            Supplier<@Nullable Profile> profileSupplier) {
        mContext = bottomBarView.getContext();
        mScrimManager = scrimManager;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mControlContainer = controlContainer;
        mBottomAppBarContainer = bottomAppBarContainer;
        mBottomBarView = bottomBarView;
        mProfileSupplier = profileSupplier;
    }

    /**
     * Destroys the coordinator, removing any active layout listeners and dismissing any shown UI.
     */
    public void destroy() {
        removeLayoutListeners();
        dismiss();
        mState = State.DONE;
    }

    /**
     * Returns whether the opt-in dialog handed over to the tooltip chain before the activity
     * recreated.
     */
    boolean hasPendingTooltips() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.BOTTOM_BAR_EDUCATION_PENDING_TOOLTIPS, false);
    }

    /**
     * @param profile The current Profile.
     * @return Whether the education IPH would be shown if requested.
     */
    public boolean wouldShow(Profile profile) {
        if (hasPendingTooltips()) return true;
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile.getOriginalProfile());
        return tracker != null
                && tracker.isInitialized()
                && tracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    /** Attempts to show the education bubble if the conditions are met. */
    public void maybeShow() {
        if (mState != State.IDLE) return;
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        if (!mBottomBarView.isLaidOut() || !mBottomBarView.isShown()) {
            waitForLayout(this::maybeShow);
            return;
        }

        if (hasPendingTooltips()) {
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.BOTTOM_BAR_EDUCATION_PENDING_TOOLTIPS);
            // Set at approval time like FET; if UI fails, tip is deferred.
            setEducationShownThisSession();
            activate(State.SHOWING_TOOLTIPS);
            startTooltipChain();
            return;
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile.getOriginalProfile());
        if (!tracker.isInitialized()) {
            tracker.addOnInitializedCallback(
                    success -> {
                        if (success && mState == State.IDLE) maybeShow();
                    });
            return;
        }

        View controlContainer = mControlContainer;
        View anchorView =
                selectScrimAnchor(
                        mBrowserControlsVisibilityManager.getControlsPosition(),
                        controlContainer,
                        mBottomAppBarContainer);
        if (anchorView == null) return;

        if (!tracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION)) {
            return;
        }

        // Set at approval time like FET; if UI fails, tip is deferred.
        setEducationShownThisSession();
        activate(State.SHOWING_BUBBLE);
        mActiveTracker = tracker;
        showScrim(anchorView);
        showBubble();
    }

    /**
     * Starts the flow in {@code state}: registers observers and keeps the browser controls shown
     * until {@link #dismiss()}.
     */
    private void activate(@State int state) {
        mState = state;
        // Record the orientation the flow is first shown in; only later changes re-show it.
        mLastOrientation = mContext.getResources().getConfiguration().orientation;
        mContext.registerComponentCallbacks(this);
        mBrowserControlsVisibilityManager.addObserver(this);
        mBrowserControlsToken =
                mBrowserControlsVisibilityManager
                        .getBrowserVisibilityDelegate()
                        .showControlsPersistent();
    }

    /** Dismisses the education bubble and hides the scrim. */
    public void dismiss() {
        if (mState != State.SHOWING_BUBBLE && mState != State.SHOWING_TOOLTIPS) return;
        mState = State.DONE;
        unregisterObservers();
        removeLayoutListeners();

        dismissBubblePopup();
        if (mActiveBubbleOnboardingType != null) {
            BottomBarMetrics.recordEducationOnboardingEvent(
                    mActiveBubbleOnboardingType, EducationOnboardingEvent.DISMISSED_OTHER);
            mActiveBubbleOnboardingType = null;
        }

        if (mCurrentTooltip != null) {
            if (mLastShownTooltipStep == mTooltipStep) {
                BottomBarMetrics.recordIphEvent(
                        IphEvent.DISMISSED,
                        getTooltipTypeForAction(TOOLTIP_ACTION_IDS[mTooltipStep]));
            }
            TextBubble tooltip = mCurrentTooltip;
            mCurrentTooltip = null;
            tooltip.dismiss();
            mLastShownTooltipStep = NO_TOOLTIP_STEP;
        }
        if (mCurrentHighlightView != null) {
            ViewHighlighter.turnOffHighlight(mCurrentHighlightView);
            mCurrentHighlightView = null;
        }

        if (mActiveTracker != null) {
            mActiveTracker.dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
            mActiveTracker = null;
        }

        hideScrim();
        if (mBrowserControlsToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsVisibilityManager
                    .getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mBrowserControlsToken);
            mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void dismissBubblePopup() {
        AnchoredPopupWindow p = mPopupWindow;
        mPopupWindow = null;
        if (p != null) p.dismiss();
    }

    static boolean shouldUseGeminiVariant(boolean geminiFocusedParam, @Nullable View glicButton) {
        return geminiFocusedParam
                && glicButton != null
                && glicButton.isShown()
                && glicButton.getWidth() > 0;
    }

    /**
     * Returns the window y-coordinate that the education bubble should sit above. With the omnibox
     * at the bottom, the toolbar in {@code controlContainer} is stacked directly above the bottom
     * bar and is also kept above the scrim, so the bubble has to clear it as well.
     */
    static int computeBubbleAnchorTop(
            @ControlsPosition int controlsPosition,
            View bottomBarView,
            @Nullable View controlContainer) {
        int[] location = new int[2];
        bottomBarView.getLocationInWindow(location);
        int top = location[1];
        if (controlsPosition == ControlsPosition.BOTTOM && controlContainer != null) {
            controlContainer.getLocationInWindow(location);
            top = Math.min(top, location[1]);
        }
        return top;
    }

    static int computeArrowOffset(
            int anchorCenterX, int popupX, int popupWidth, int leftSpacing, int rightSpacing) {
        int offset = anchorCenterX - popupX;
        offset = Math.max(leftSpacing, offset);
        offset = Math.min(popupWidth - rightSpacing, offset);
        return offset;
    }

    static @Nullable View selectAnchorTarget(
            boolean geminiFocused, @Nullable View glicButton, @Nullable View bottomBarView) {
        if (geminiFocused && glicButton != null) {
            return glicButton;
        }
        return bottomBarView;
    }

    private static @Nullable View selectScrimAnchor(
            @ControlsPosition int controlsPosition,
            @Nullable View controlContainer,
            @Nullable View bottomAppBarContainer) {
        return controlsPosition == ControlsPosition.BOTTOM
                ? controlContainer
                : bottomAppBarContainer;
    }

    private void showScrim(View anchorView) {
        mScrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, anchorView)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.AFFECTS_NAVIGATION_BAR, false)
                        .with(
                                ScrimProperties.BACKGROUND_COLOR,
                                mContext.getColor(R.color.default_scrim_color))
                        .with(
                                ScrimProperties.BOTTOM_MARGIN,
                                mBrowserControlsVisibilityManager.getBottomControlsHeight())
                        .with(ScrimProperties.CLICK_DELEGATE, CallbackUtils.emptyRunnable())
                        .build();

        mScrimManager.showScrim(mScrimModel);
    }

    // ClickableViewAccessibility is suppressed because setTouchInterceptor only consumes touches
    // outside the popup bounds to keep the bubble modal without triggering a click action.
    @SuppressLint("ClickableViewAccessibility")
    private void showBubble() {
        Profile profile = mProfileSupplier.get();
        boolean glicFocusedParam =
                profile != null && BottomBarEducationUtils.shouldUseGlicVariant(profile);
        View glicButton = mBottomBarView.getContainerForAction(ActionId.GLIC);
        boolean geminiFocused = shouldUseGeminiVariant(glicFocusedParam, glicButton);

        Runnable openSettingsAndDismiss =
                () -> {
                    String onboardingType = mActiveBubbleOnboardingType;
                    mActiveBubbleOnboardingType = null;
                    if (onboardingType != null) {
                        BottomBarMetrics.recordEducationOnboardingEvent(
                                onboardingType, EducationOnboardingEvent.SETTINGS_CLICKED);
                    }
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(
                                    mContext, SettingsNavigation.SettingsFragment.APPEARANCE);
                    dismiss();
                };

        Runnable dismissAndTurnOffBottomBar =
                () -> {
                    String onboardingType = mActiveBubbleOnboardingType;
                    mActiveBubbleOnboardingType = null;
                    if (onboardingType != null) {
                        BottomBarMetrics.recordEducationOnboardingEvent(
                                onboardingType, EducationOnboardingEvent.DECLINED);
                    }
                    // Dismiss before writing the pref: pref listeners are notified synchronously,
                    // and ChromeTabbedActivity recreates itself without the bottom bar in response.
                    dismiss();
                    BottomBarConfigUtils.setBottomBarUserEnabled(/* enabled= */ false);
                };

        Runnable onShowMeClicked =
                () -> {
                    String onboardingType = mActiveBubbleOnboardingType;
                    mActiveBubbleOnboardingType = null;
                    if (onboardingType != null) {
                        BottomBarMetrics.recordEducationOnboardingEvent(
                                onboardingType, EducationOnboardingEvent.ACCEPTED);
                    }
                    if (mActiveTracker != null) {
                        mActiveTracker.notifyEvent(
                                EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_USED);
                    }
                    mState = State.SHOWING_TOOLTIPS;
                    hideScrim();
                    dismissBubblePopup();
                    startTooltipChain();
                };

        View bubbleView =
                BottomBarEducationUtils.inflateBubbleContent(
                        mContext,
                        geminiFocused,
                        /* onSettingsClicked= */ openSettingsAndDismiss,
                        /* onPositiveClicked= */ onShowMeClicked,
                        /* onNegativeClicked= */ dismissAndTurnOffBottomBar);

        // Determine the horizontal anchor target. For the default variant, the bubble and its
        // arrow point to the center of the bottom bar. For the Gemini-focused variant, they
        // point to the Gemini (GLIC) button.
        final View arrowTarget = selectAnchorTarget(geminiFocused, glicButton, mBottomBarView);

        final int anchorGapPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_education_bubble_anchor_gap);
        RectProvider rectProvider =
                new RectProvider() {
                    private final View.OnLayoutChangeListener mListener =
                            (_, _, _, _, _, _, _, _, _) -> notifyRectChanged();

                    @Override
                    public void startObserving(Observer observer) {
                        super.startObserving(observer);
                        if (mControlContainer != null) {
                            mControlContainer.addOnLayoutChangeListener(mListener);
                        }
                        mBottomBarView.addOnLayoutChangeListener(mListener);
                    }

                    @Override
                    public void stopObserving() {
                        if (mControlContainer != null) {
                            mControlContainer.removeOnLayoutChangeListener(mListener);
                        }
                        mBottomBarView.removeOnLayoutChangeListener(mListener);
                        super.stopObserving();
                    }

                    @Override
                    public Rect getRect() {
                        int[] bottomLoc = new int[2];
                        mBottomBarView.getLocationInWindow(bottomLoc);
                        int bottom = bottomLoc[1] + mBottomBarView.getHeight();
                        int top =
                                computeBubbleAnchorTop(
                                        mBrowserControlsVisibilityManager.getControlsPosition(),
                                        mBottomBarView,
                                        mControlContainer);

                        int[] targetLoc = new int[2];
                        if (arrowTarget != null) {
                            arrowTarget.getLocationInWindow(targetLoc);
                        }
                        int targetCenterX =
                                arrowTarget != null ? targetLoc[0] + arrowTarget.getWidth() / 2 : 0;
                        return new Rect(targetCenterX, top - anchorGapPx, targetCenterX, bottom);
                    }
                };

        ArrowBubbleDrawable arrow = new ArrowBubbleDrawable(mContext, /* isRoundBubble= */ false);
        arrow.setShowArrow(true);
        arrow.setCornerRadiusPx(ViewUtils.dpToPx(mContext, 16));
        arrow.setBubbleColor(SemanticColorUtils.getMenuBgColor(mContext));

        View rootView = mBottomBarView.getRootView();
        mPopupWindow =
                new AnchoredPopupWindow.Builder(
                                mContext, rootView, arrow, () -> bubbleView, rectProvider)
                        .setPreferredHorizontalOrientation(
                                AnchoredPopupWindow.HorizontalOrientation.CENTER)
                        .setPreferredVerticalOrientation(
                                AnchoredPopupWindow.VerticalOrientation.ABOVE)
                        .setLayoutObserver(
                                (positionBelow, x, y, width, height, anchorRect) -> {
                                    int arrowOffset =
                                            computeArrowOffset(
                                                    anchorRect.centerX(),
                                                    x,
                                                    width,
                                                    arrow.getArrowLeftSpacing(),
                                                    arrow.getArrowRightSpacing());
                                    arrow.setPositionProperties(arrowOffset, positionBelow);
                                })
                        .setFocusable(true)
                        .setTouchModal(true)
                        .setDismissOnTouchInteraction(false)
                        .setTouchInterceptor(
                                (v, event) -> {
                                    int action = event.getActionMasked();
                                    if (action == MotionEvent.ACTION_OUTSIDE) return true;
                                    if (action == MotionEvent.ACTION_DOWN) {
                                        float x = event.getX();
                                        float y = event.getY();
                                        return x < 0
                                                || y < 0
                                                || x >= v.getWidth()
                                                || y >= v.getHeight();
                                    }
                                    return false;
                                })
                        .addOnDismissListener(this::onPopupDismissed)
                        .setMargin(
                                mContext.getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.bottom_bar_education_bubble_side_margin))
                        .setDesiredContentWidth(
                                rootView.getWidth() > 0
                                        ? rootView.getWidth()
                                        : mContext.getResources().getDisplayMetrics().widthPixels)
                        .build();
        mPopupWindow.show();
        if (!mPopupWindow.isShowing()) {
            dismiss();
            return;
        }

        if (mActiveBubbleOnboardingType == null) {
            mActiveBubbleOnboardingType =
                    geminiFocused
                            ? EducationOnboardingType.GEMINI_BUBBLE
                            : EducationOnboardingType.DEFAULT_BUBBLE;
            BottomBarMetrics.recordEducationOnboardingEvent(
                    mActiveBubbleOnboardingType, EducationOnboardingEvent.SHOWN);
        }
    }

    private static @IphFeature String getTooltipTypeForAction(@ActionId int actionId) {
        switch (actionId) {
            case ActionId.GLIC:
                return IphFeature.GLIC;
            case ActionId.NEW_TAB:
                return IphFeature.NEW_TAB;
            case ActionId.TAB_SWITCHER:
                return IphFeature.TAB_SWITCHER;
            default:
                throw new IllegalStateException("No IPH feature for tooltip action " + actionId);
        }
    }

    private void startTooltipChain() {
        mTooltipStep = 0;
        mLastShownTooltipStep = NO_TOOLTIP_STEP;
        showCurrentTooltip();
    }

    private void showCurrentTooltip() {
        if (mState != State.SHOWING_TOOLTIPS) return;

        while (mTooltipStep < TOOLTIP_ACTION_IDS.length) {
            int actionId = TOOLTIP_ACTION_IDS[mTooltipStep];
            BottomBarButtonContainer container = mBottomBarView.getContainerForAction(actionId);
            if (container != null
                    && container.isShown()
                    && container.hasTargetView()
                    && container.getTargetView().isShown()) {
                break;
            }
            mTooltipStep++;
        }

        if (mTooltipStep >= TOOLTIP_ACTION_IDS.length) {
            dismiss();
            return;
        }

        int actionId = TOOLTIP_ACTION_IDS[mTooltipStep];
        int stringId = TOOLTIP_STRING_IDS[mTooltipStep];
        BottomBarButtonContainer container = mBottomBarView.getContainerForAction(actionId);
        assert container != null;
        View anchor = container.getTargetView();

        mCurrentHighlightView = anchor;
        HighlightParams highlightParams =
                BottomBarUtils.createButtonHighlightParams(
                        mContext,
                        actionId == ActionId.NEW_TAB && mBottomBarView.isNewTabBackgroundVisible());
        ViewHighlighter.turnOnHighlight(mCurrentHighlightView, highlightParams);

        View rootView = mBottomBarView.getRootView();
        mCurrentTooltip =
                new TextBubble.Builder(
                                mContext,
                                rootView,
                                new ViewRectProvider(anchor),
                                stringId,
                                stringId)
                        .build();
        mCurrentTooltip.setDismissOnTouchInteraction(true);
        TextBubble tooltip = mCurrentTooltip;
        mCurrentTooltip.addOnDismissListener(
                () -> {
                    if (mCurrentTooltip != tooltip) return;
                    BottomBarMetrics.recordIphEvent(
                            IphEvent.DISMISSED, getTooltipTypeForAction(actionId));

                    if (mCurrentHighlightView != null) {
                        ViewHighlighter.turnOffHighlight(mCurrentHighlightView);
                        mCurrentHighlightView = null;
                    }
                    mCurrentTooltip = null;
                    mTooltipStep++;
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (mState == State.SHOWING_TOOLTIPS) showCurrentTooltip();
                            });
                });
        mCurrentTooltip.show();
        if (!sSkipTooltipShowCheckForTesting && !mCurrentTooltip.isShowing()) {
            dismiss();
            return;
        }

        if (mLastShownTooltipStep != mTooltipStep) {
            mLastShownTooltipStep = mTooltipStep;
            BottomBarMetrics.recordIphEvent(IphEvent.SHOWN, getTooltipTypeForAction(actionId));
        }
    }

    /**
     * Skips the post-show isShowing() check; Robolectric measures short tooltips below the popup's
     * minimum size.
     */
    static void setSkipTooltipShowCheckForTesting(boolean skip) {
        boolean previous = sSkipTooltipShowCheckForTesting;
        sSkipTooltipShowCheckForTesting = skip;
        ResettersForTesting.register(() -> sSkipTooltipShowCheckForTesting = previous);
    }

    /**
     * Runs {@code action} once the bottom bar is laid out and shown. Uses a one-shot {@link
     * ViewTreeObserver.OnGlobalLayoutListener} because {@link ViewTreeObserver} dispatches global
     * layout callbacks both after layout passes and when an ancestor changes visibility between
     * {@link View#INVISIBLE} and {@link View#VISIBLE} without re-laying out the bar.
     */
    // TODO(crbug.com/567168121): Replace OnGlobalLayoutListener with explicit bottom controls
    // Android view visibility signaling in next refactor.
    private void waitForLayout(Runnable action) {
        if (mGlobalLayoutListener != null) return;
        mGlobalLayoutListener =
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        if (!mBottomBarView.isLaidOut() || !mBottomBarView.isShown()) return;
                        removeLayoutListeners();
                        action.run();
                    }
                };
        mBottomBarView.getViewTreeObserver().addOnGlobalLayoutListener(mGlobalLayoutListener);
    }

    private void unregisterObservers() {
        mContext.unregisterComponentCallbacks(this);
        mBrowserControlsVisibilityManager.removeObserver(this);
    }

    private void removeLayoutListeners() {
        if (mGlobalLayoutListener != null) {
            mBottomBarView
                    .getViewTreeObserver()
                    .removeOnGlobalLayoutListener(mGlobalLayoutListener);
            mGlobalLayoutListener = null;
        }
    }

    private void hideScrim() {
        if (mScrimModel != null) {
            mScrimManager.hideScrim(mScrimModel, /* animate= */ false);
            mScrimModel = null;
        }
    }

    private void onPopupDismissed() {
        if (mPopupWindow != null && mState == State.SHOWING_BUBBLE) {
            dismiss();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (newConfig.orientation == mLastOrientation) return;
        mLastOrientation = newConfig.orientation;
        recreate();
    }

    /** Re-anchors the currently shown UI after an orientation change, keeping the flow active. */
    private void recreate() {
        if (mState == State.SHOWING_BUBBLE) {
            if (mPopupWindow == null) return;
            dismissBubblePopup();
            waitForLayout(this::showBubble);
        } else if (mState == State.SHOWING_TOOLTIPS) {
            if (mCurrentTooltip == null) return;
            TextBubble tooltip = mCurrentTooltip;
            mCurrentTooltip = null;
            tooltip.dismiss(); // listener ignores it: mCurrentTooltip != tooltip
            if (mCurrentHighlightView != null) {
                ViewHighlighter.turnOffHighlight(mCurrentHighlightView);
                mCurrentHighlightView = null;
            }
            waitForLayout(this::showCurrentTooltip); // same mTooltipStep -> same tooltip
        }
    }

    @Override
    public void onLowMemory() {}

    @Override
    public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
        dismiss();
    }

    @Nullable View getBubbleViewForTesting() {
        return mPopupWindow != null ? mPopupWindow.getContentView() : null;
    }

    @Nullable TextBubble getCurrentTooltipForTesting() {
        return mCurrentTooltip;
    }

    /**
     * Records that bottom bar education has shown this session (process lifetime), which defers
     * (doesn't drop) the NTP theme tip. It's never cleared until the process dies.
     */
    static void setEducationShownThisSession() {
        if (sEducationShownThisSession) return;
        sEducationShownThisSession = true;
        ResettersForTesting.register(() -> sEducationShownThisSession = false);
    }

    /** Returns whether bottom bar education has shown this session. */
    public static boolean hasEducationShownThisSession() {
        return sEducationShownThisSession;
    }

    /** Marks bottom bar education as shown this session. Reset after each test. */
    public static void setEducationShownThisSessionForTesting() {
        setEducationShownThisSession();
    }
}
