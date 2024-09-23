// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RotateDrawable;
import android.os.Build;
import android.os.Build.VERSION;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.TooltipCompat;

import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.widget.ChromeTransitionDrawable;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** StatusView is a location bar's view displaying status (icons and/or text). */
public class StatusView extends LinearLayout {
    @IntDef({IconTransitionType.CROSSFADE, IconTransitionType.ROTATE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconTransitionType {
        int CROSSFADE = 0;
        int ROTATE = 1;
    }

    public static final int ICON_ANIMATION_DURATION_MS = 225;
    public static final int ICON_ROTATION_DURATION_MS = 250;
    private static final int ICON_ROTATION_DEGREES = 180;

    private @Nullable View mIncognitoBadge;
    // The default value is 0, which matches R.dimen.location_bar_start_padding.
    private int mTouchDelegateStartOffset;
    private int mTouchDelegateEndOffset;

    private ImageView mIconView;
    private View mIconBackground;
    private StatusIconView mStatusIconView;
    private TextView mVerboseStatusTextView;
    private View mSeparatorView;
    private View mStatusExtraSpace;

    private boolean mAnimationsEnabled;
    private boolean mAnimatingStatusIconShow;
    private boolean mAnimatingStatusIconHide;
    private boolean mIsAnimatingStatusIconChange;

    private @StringRes int mAccessibilityToast;
    private @StringRes int mAccessibilityDoubleTapDescription;

    private Drawable mStatusIconDrawable;

    private TouchDelegate mTouchDelegate;
    private CompositeTouchDelegate mCompositeTouchDelegate;

    private boolean mLastTouchDelegateRtlness;
    private Rect mLastTouchDelegateRect;

    private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private int mShowBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private Integer mIconAnimationDurationForTests;

    public StatusView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = findViewById(R.id.location_bar_status_icon);
        mIconBackground = findViewById(R.id.location_bar_status_icon_bg);
        mStatusIconView = findViewById(R.id.location_bar_status_icon_view);
        mVerboseStatusTextView = findViewById(R.id.location_bar_verbose_status);
        mSeparatorView = findViewById(R.id.location_bar_verbose_status_separator);
        mStatusExtraSpace = findViewById(R.id.location_bar_verbose_status_extra_space);

        // Set onHoverListener for verbose status view to hide the divider while the verbose hover
        // highlight is showing.
        setOnHoverListener(
                new OnHoverListener() {
                    private int mSeparatorVisibility;

                    @Override
                    public boolean onHover(View v, MotionEvent event) {
                        switch (event.getAction()) {
                            case MotionEvent.ACTION_HOVER_ENTER:
                                mSeparatorVisibility = mSeparatorView.getVisibility();
                                if (getBackground() != null
                                        && mSeparatorVisibility == View.VISIBLE) {
                                    mSeparatorView.setVisibility(View.GONE);
                                }
                                return false;
                            case MotionEvent.ACTION_HOVER_EXIT:
                                if (mSeparatorView.getVisibility() != mSeparatorVisibility) {
                                    mSeparatorView.setVisibility(mSeparatorVisibility);
                                }
                                return false;
                            default:
                                return false;
                        }
                    }
                });

        // Configure icon rounding.
        mIconView.setOutlineProvider(
                new RoundedCornerOutlineProvider(
                        getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.omnibox_search_engine_logo_composed_size)
                                / 2));
        mIconView.setClipToOutline(true);

        configureAccessibilityDescriptions();
    }

    /**
     * Set tooltip text resource id.
     *
     * @param tooltipTextResId tooltip text resource id.
     */
    public void setTooltipText(@StringRes int tooltipTextResId) {
        if (tooltipTextResId != Resources.ID_NULL) {
            setTooltipText(mStatusIconView.getContext().getString(tooltipTextResId));
        } else {
            setTooltipText(null);
        }
    }

    /**
     * Set hover highlight resource id.
     *
     * @param hoverHighlightResId background hover highlight resource id.
     */
    public void setHoverHighlight(@DrawableRes int hoverHighlightResId) {
        if (hoverHighlightResId != Resources.ID_NULL && isSearchEngineStatusIconVisible()) {
            setBackground(AppCompatResources.getDrawable(getContext(), hoverHighlightResId));
        } else {
            setBackground(null);
        }
    }

    /** Return whether search engine status icon is visible. */
    public boolean isSearchEngineStatusIconVisible() {
        return mStatusIconView.getIconVisibility() == VISIBLE;
    }

    /**
     * Set the composite touch delegate here to which this view's touch delegate will be added.
     *
     * @param compositeTouchDelegate The parent's CompositeTouchDelegate to be used.
     */
    public void setCompositeTouchDelegate(CompositeTouchDelegate compositeTouchDelegate) {
        mCompositeTouchDelegate = compositeTouchDelegate;
        mIconView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                        updateTouchDelegate());
    }

    /**
     * Start animating transition of status icon.
     *
     * @param transitionType The animation transition type for the icon.
     * @param animationFinishedCallback The callback to be run after the status icon has been
     *     successfully set.
     */
    private void animateStatusIcon(
            @IconTransitionType int transitionType, @Nullable Runnable animationFinishedCallback) {
        Drawable targetIcon = mStatusIconDrawable;
        boolean wantIconHidden = mStatusIconDrawable == null;
        // We only handle additional callback after rotation.
        assert transitionType == IconTransitionType.ROTATE || animationFinishedCallback == null;

        // Ensure proper handling of animations.
        // Possible variants:
        // 1. Is: shown,           want: hidden  => animate hiding,
        // 2. Is: shown,           want: shown   => crossfade w/ChromeTransitionDrawable,
        // 3. Is: animating(show), want: hidden  => cancel animation; animate hiding,
        // 4. Is: animating(show), want: shown   => crossfade (carry on showing),
        // 5. Is: animating(hide), want: hidden  => no op,
        // 6. Is: animating(hide), want: shown   => cancel animation; animate showing; crossfade,
        // 7. Is: hidden,          want: hidden  => no op,
        // 8. Is: hidden,          want: shown   => animate showing.
        //
        // This gives 3 actions:
        // - Animate showing, if hidden or previously hiding (6 + 8); cancel previous animation (6)
        // - Animate hiding, if shown or previously showing (1 + 3); cancel previous animation (3)
        // - crossfade w/ChromeTransitionDrawable, if visible (2, 4, 6), otherwise use image
        // directly. All other options (5, 7) are no-op.
        //
        // Note: this will be compacted once we start using LayoutTransition with StatusView.
        boolean isIconHidden = mStatusIconView.getIconVisibility() == View.GONE;
        if (!wantIconHidden && (isIconHidden || mAnimatingStatusIconHide)) {
            // Action 1: animate showing, if icon was either hidden or hiding.
            if (mAnimatingStatusIconHide) mIconView.animate().cancel();
            mAnimatingStatusIconHide = false;
            mAnimatingStatusIconShow = true;
            keepControlsShownForAnimation();

            // Set StatusIcon visibility and check whether we should set hover action on StatusView.
            setStatusIconVisibility(View.VISIBLE);

            mIconView
                    .animate()
                    .alpha(1.0f)
                    .setDuration(getIconAnimationDuration())
                    .withEndAction(
                            () -> {
                                mAnimatingStatusIconShow = false;
                                allowBrowserControlsHide();
                                // Wait until the icon is visible so the bounds will be properly
                                // set.
                                updateTouchDelegate();
                            })
                    .start();
        } else if (wantIconHidden && (!isIconHidden || mAnimatingStatusIconShow)) {
            // Action 2: animate hiding, if icon was either shown or showing.
            if (mAnimatingStatusIconShow) mIconView.animate().cancel();
            mAnimatingStatusIconShow = false;
            mAnimatingStatusIconHide = true;
            keepControlsShownForAnimation();
            // Do not animate phase-out when animations are disabled.
            // While this looks nice in some cases (navigating to insecure sites),
            // it has a side-effect of briefly showing padlock (phase-out) when navigating
            // back and forth between secure and insecure sites, which seems like a glitch.
            // See bug: crbug.com/919449
            mIconView
                    .animate()
                    .setDuration(mAnimationsEnabled ? getIconAnimationDuration() : 0)
                    .alpha(0.0f)
                    .withEndAction(
                            () -> {
                                // Set StatusIcon visibility and check whether we should set hover
                                // action on StatusView.
                                setStatusIconVisibility(View.GONE);

                                mIconView.setAlpha(1f);
                                mAnimatingStatusIconHide = false;
                                allowBrowserControlsHide();
                                updateTouchDelegate();
                            })
                    .start();
        } else {
            updateTouchDelegate();
        }

        // Action 3: Specify icon content. Use ChromeTransitionDrawable whenever object is visible.
        if (targetIcon != null) {
            if (!isIconHidden) {
                Drawable existingDrawable = mIconView.getDrawable();
                if (existingDrawable instanceof ChromeTransitionDrawable) {
                    ChromeTransitionDrawable transitionDrawable =
                            (ChromeTransitionDrawable) existingDrawable;
                    // Finish any running animations in the existing drawable because we're going to
                    // reuse it. Concurrent animations could clobber each other's changes and cause
                    // inconsistent states.
                    transitionDrawable.finishTransition(true);
                    existingDrawable = transitionDrawable.getFinalDrawable();
                }

                ChromeTransitionDrawable newImage =
                        new ChromeTransitionDrawable(
                                existingDrawable,
                                transitionType == IconTransitionType.ROTATE
                                        ? getRotatedIcon(targetIcon)
                                        : targetIcon);
                mIconView.setImageDrawable(newImage);

                if (transitionType == IconTransitionType.CROSSFADE) {
                    mIsAnimatingStatusIconChange = true;
                    long duration = mAnimationsEnabled ? getIconAnimationDuration() : 0;
                    if (duration > 0) {
                        keepControlsShownForAnimation();
                    }
                    newImage.setCrossFadeEnabled(true);
                    newImage.startTransition()
                            .setDuration(duration)
                            .withEndAction(this::resetAnimationStatus);
                } else {
                    mIsAnimatingStatusIconChange = true;
                    keepControlsShownForAnimation();
                    mIconView
                            .animate()
                            .setDuration(ICON_ROTATION_DURATION_MS)
                            .rotationBy(ICON_ROTATION_DEGREES)
                            .setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR)
                            .withStartAction(
                                    () -> {
                                        newImage.startTransition()
                                                .setDuration(getIconAnimationDuration())
                                                .withEndAction(this::resetAnimationStatus);
                                    })
                            .withEndAction(
                                    () -> {
                                        mIsAnimatingStatusIconChange = false;
                                        allowBrowserControlsHide();
                                        mIconView.setRotation(0);
                                        // Only update status icon if it is still the current icon.
                                        if (mStatusIconDrawable == targetIcon) {
                                            mIconView.setImageDrawable(targetIcon);
                                            if (animationFinishedCallback != null) {
                                                animationFinishedCallback.run();
                                            }
                                        }
                                    })
                            .start();
                }

                // Update the touch delegate only if the icons are swapped without animating the
                // image view.
                if (!mAnimatingStatusIconShow) updateTouchDelegate();
            } else {
                mIconView.setImageDrawable(targetIcon);
            }
        }
    }

    private void setStatusIconVisibility(int visibility) {
        mStatusIconView.setVisibility(visibility);
    }

    /** Returns a rotated version of the icon passed in. */
    private Drawable getRotatedIcon(@NonNull Drawable icon) {
        RotateDrawable rotated = new RotateDrawable();
        rotated.setDrawable(icon);
        rotated.setToDegrees(ICON_ROTATION_DEGREES);
        // Jump drawable to its target state.
        rotated.setLevel(10000);
        return rotated;
    }

    /**
     * Specify object to receive click events.
     *
     * @param listener Instance of View.OnClickListener or null.
     */
    void setStatusClickListener(View.OnClickListener listener) {
        setOnClickListener(listener);
    }

    /** Configure accessibility descriptions. */
    void configureAccessibilityDescriptions() {
        View.OnLongClickListener listener =
                new View.OnLongClickListener() {
                    @Override
                    public boolean onLongClick(View view) {
                        if (mAccessibilityToast == 0) return false;
                        Context context = getContext();
                        return Toast.showAnchoredToast(
                                context,
                                view,
                                context.getResources().getString(mAccessibilityToast));
                    }
                };
        setOnLongClickListener(listener);

        setAccessibilityDelegate(
                new AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);

                        if (mAccessibilityDoubleTapDescription == 0) return;

                        String onTapDescription =
                                getContext()
                                        .getResources()
                                        .getString(mAccessibilityDoubleTapDescription);
                        info.addAction(
                                new AccessibilityAction(
                                        AccessibilityNodeInfo.ACTION_CLICK, onTapDescription));
                    }
                });
    }

    /** Toggle use of animations. */
    void setAnimationsEnabled(boolean enabled) {
        mAnimationsEnabled = enabled;
    }

    /**
     * Sets the Drawable to be used as the status icon.
     *
     * @param statusIconDrawable The Drawable.
     * @param transitionType The animation transition type for the icon.
     * @param animationFinishedCallback The callback to be run after the new drawable has been
     *     successfully set.
     */
    void setStatusIconResources(
            @Nullable Drawable statusIconDrawable,
            @IconTransitionType int transitionType,
            @Nullable Runnable animationFinishedCallback) {
        mStatusIconDrawable = statusIconDrawable;
        animateStatusIcon(transitionType, animationFinishedCallback);
    }

    /** Specify the status icon alpha. */
    void setStatusIconAlpha(float alpha) {
        if (mIconView == null) return;
        mIconView.setAlpha(alpha);

        if (mIconBackground != null && mIconBackground.getVisibility() == VISIBLE) {
            mIconBackground.setAlpha(alpha);
        }
    }

    /** Specify the status icon visibility. */
    public void setStatusIconShown(boolean showIcon) {
        if (mStatusIconView == null) return;
        // Check if layout was requested before changing our child view.
        boolean wasLayoutPreviouslyRequested = isLayoutRequested();

        // Set StatusIcon visibility and check whether we should set hover action on StatusView.
        setStatusIconVisibility(showIcon ? VISIBLE : GONE);

        updateTouchDelegate();
        if (mIsAnimatingStatusIconChange && !showIcon) {
            // If the icon view is hidden before it gets a chance to draw, our animation status will
            // become stale. Reset it.
            resetAnimationStatus();
        }

        // If the icon's visibility changes while layout is pending, we can end up in a bad state
        // due to a stale measurement cache. Post a task to request layout to force this visibility
        // change (crbug.com/1345552).
        if (wasLayoutPreviouslyRequested && getHandler() != null) {
            getHandler()
                    .post(
                            () ->
                                    ViewUtils.requestLayout(
                                            this, "StatusView.setStatusIconShown Runnable"));
        }
    }

    /** Specify the status icon background visibility. */
    void setStatusIconBackgroundVisibility(boolean showIconBackground) {
        if (mIconView == null || mIconBackground == null) return;

        mIconBackground.setVisibility(showIconBackground ? VISIBLE : INVISIBLE);
    }

    /** Specify accessibility string presented to user upon long click. */
    void setStatusAccessibilityToast(@StringRes int description) {
        mAccessibilityToast = description;
    }

    /** Specify accessibility string used for "Double tap to" description. */
    void setStatusAccessibilityDoubleTapDescription(@StringRes int description) {
        mAccessibilityDoubleTapDescription = description;
    }

    /** Specify content description for security icon. */
    void setStatusIconDescription(@StringRes int descriptionRes) {
        String description = null;
        int importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO;
        if (descriptionRes != 0) {
            description = getResources().getString(descriptionRes);
            importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_YES;
        }
        mIconView.setContentDescription(description);
        setImportantForAccessibility(importantForAccessibility);
    }

    /** Select color of Separator view. */
    void setSeparatorColor(@ColorInt int separatorColor) {
        mSeparatorView.setBackgroundColor(separatorColor);
    }

    /** Select color of verbose status text. */
    void setVerboseStatusTextColor(@ColorInt int textColor) {
        mVerboseStatusTextView.setTextColor(textColor);
    }

    /** Specify content of the verbose status text. */
    void setVerboseStatusTextContent(@StringRes int content) {
        mVerboseStatusTextView.setText(content);
    }

    /** Specify visibility of the verbose status text. */
    public void setVerboseStatusTextVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        mVerboseStatusTextView.setVisibility(visibility);
        mSeparatorView.setVisibility(visibility);
        mStatusExtraSpace.setVisibility(visibility);
    }

    /** Specify width of the verbose status text. */
    void setVerboseStatusTextWidth(int width) {
        mVerboseStatusTextView.setMaxWidth(width);
    }

    /**
     * @param incognitoBadgeVisible Whether or not the incognito badge is visible.
     */
    void setIncognitoBadgeVisibility(boolean incognitoBadgeVisible) {
        // Initialize the incognito badge on the first time it becomes visible.
        if (mIncognitoBadge == null && !incognitoBadgeVisible) return;
        if (mIncognitoBadge == null) initializeIncognitoBadge();

        mIncognitoBadge.setVisibility(incognitoBadgeVisible ? View.VISIBLE : View.GONE);
        updateTouchDelegate();
    }

    void setBrowserControlsVisibilityDelegate(
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate) {
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
    }

    private void initializeIncognitoBadge() {
        ViewStub viewStub = findViewById(R.id.location_bar_incognito_badge_stub);
        mIncognitoBadge = viewStub.inflate();
    }

    /**
     * Create a touch delegate to expand the clickable area for the padlock icon (see
     * crbug.com/970031 for motivation/info). This method will be called when the icon is animating
     * in and when layout changes. It's called on these intervals because
     *
     * <ul>
     *   <li>the layout could change and
     *   <li>the Rtl-ness of the view could change. There are checks in place to avoid doing
     *       unnecessary work, so if the rect is empty or equivalent to the one already in place, no
     *       work will be done.
     * </ul>
     */
    private void updateTouchDelegate() {
        if (mCompositeTouchDelegate == null) return;

        if (!isIconVisible()) {
            // Tear down the existing delegate if it exists.
            if (mTouchDelegate != null) {
                mCompositeTouchDelegate.removeDelegateForDescendantView(mTouchDelegate);
                mTouchDelegate = null;
                mLastTouchDelegateRect = new Rect();
            }
            return;
        }
        // Setup a touch delegate to increase the clickable area for the padlock.
        // See for more information.
        Rect touchDelegateBounds = new Rect();
        mIconView.getHitRect(touchDelegateBounds);
        if (touchDelegateBounds.equals(new Rect())) return;

        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if (mTouchDelegateEndOffset == 0) {
            mTouchDelegateEndOffset =
                    getResources().getDimensionPixelSize(R.dimen.location_bar_icon_margin_end);
        }
        touchDelegateBounds.left -= isRtl ? mTouchDelegateEndOffset : mTouchDelegateStartOffset;
        touchDelegateBounds.right += isRtl ? mTouchDelegateStartOffset : mTouchDelegateEndOffset;
        // Increase the delegate area height for tablets to satisfy minimum size requirements.
        // Ideally, we want to address crbug.com/1320384 to satisfy minimum size requirements.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            touchDelegateBounds.top -=
                    getResources()
                            .getDimensionPixelSize(
                                    R.dimen.modern_toolbar_background_vertical_offset);
            touchDelegateBounds.bottom +=
                    getResources()
                            .getDimensionPixelSize(
                                    R.dimen.modern_toolbar_background_vertical_offset);
        }

        // If our rect and rtl-ness hasn't changed, there's no need to recreate the TouchDelegate.
        if (mTouchDelegate != null
                && touchDelegateBounds.equals(mLastTouchDelegateRect)
                && mLastTouchDelegateRtlness == isRtl) {
            return;
        }
        mLastTouchDelegateRect = touchDelegateBounds;

        // Remove the existing delegate when we recreate a new one.
        if (mTouchDelegate != null) {
            mCompositeTouchDelegate.removeDelegateForDescendantView(mTouchDelegate);
        }

        // Set the delegate on LocationBarLayout because it has available space. Setting on
        // status view itself will clip the rect.
        mTouchDelegate = new TouchDelegate(touchDelegateBounds, mIconView);
        mCompositeTouchDelegate.addDelegateForDescendantView(mTouchDelegate);
        mLastTouchDelegateRtlness = isRtl;
    }

    // TODO(ender): The final last purpose of this method is to allow
    // ToolbarButtonInProductHelpController set up help bubbles. This dependency is about to
    // change. Do not depend on this method when creating new code.
    View getSecurityView() {
        return mIconView;
    }

    /**
     * @return The width of the status icon including start/end margins.
     */
    int getStatusIconWidth() {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) getLayoutParams();
        return lp.getMarginStart() + getMeasuredWidth() + lp.getMarginEnd();
    }

    boolean isStatusIconAnimating() {
        return mAnimatingStatusIconShow || mAnimatingStatusIconHide || mIsAnimatingStatusIconChange;
    }

    /**
     * @return True if the status icon is currently visible.
     */
    private boolean isIconVisible() {
        return mStatusIconDrawable != null
                && mStatusIconView.getIconVisibility() != GONE
                && mIconView.getAlpha() != 0;
    }

    /** Set tooltip text on StatusView for API >= 26. */
    private void setTooltipText(String tooltip) {
        if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TooltipCompat.setTooltipText((View) this, tooltip);
        }
    }

    private void keepControlsShownForAnimation() {
        // isShown() being false implies that the status view isn't visible. We don't want to force
        // it back into visibility just so that we can show an animation.
        if (isShown() && mBrowserControlsVisibilityDelegate != null) {
            mShowBrowserControlsToken =
                    mBrowserControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mShowBrowserControlsToken);
        }
    }

    private void allowBrowserControlsHide() {
        if (mBrowserControlsVisibilityDelegate != null) {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(
                    mShowBrowserControlsToken);
            mShowBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void resetAnimationStatus() {
        mIsAnimatingStatusIconChange = false;
        allowBrowserControlsHide();
    }

    private int getIconAnimationDuration() {
        return mIconAnimationDurationForTests == null
                ? ICON_ANIMATION_DURATION_MS
                : mIconAnimationDurationForTests;
    }

    TouchDelegate getTouchDelegateForTesting() {
        return mTouchDelegate;
    }

    void setIconAnimationDurationForTesting(int duration) {
        mIconAnimationDurationForTests = duration;
    }
}
