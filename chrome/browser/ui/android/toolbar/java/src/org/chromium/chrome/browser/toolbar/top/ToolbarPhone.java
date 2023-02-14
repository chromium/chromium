// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewDebug;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.graphics.drawable.DrawableWrapperCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.KeyboardNavigationListener;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.UrlExpansionObserver;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Phone specific toolbar implementation.
 */
public class ToolbarPhone extends ToolbarLayout implements OnClickListener, TabCountObserver {
    /** The amount of time transitioning from one theme color to another should take in ms. */
    public static final long THEME_COLOR_TRANSITION_DURATION = 250;

    public static final int URL_FOCUS_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS = 100;
    private static final int URL_CLEAR_FOCUS_TABSTACK_DELAY_MS = 200;
    private static final int URL_CLEAR_FOCUS_MENU_DELAY_MS = 250;

    // Values used during animation to show/hide optional toolbar button.
    public static final int LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final float UNINITIALIZED_FRACTION = -1f;

    /** States that the toolbar can be in regarding the tab switcher. */
    protected static final int STATIC_TAB = 0;
    protected static final int TAB_SWITCHER = 1;
    protected static final int ENTERING_TAB_SWITCHER = 2;
    protected static final int EXITING_TAB_SWITCHER = 3;

    // clang-format off
    @ViewDebug.ExportedProperty(category = "chrome", mapping = {
            @ViewDebug.IntToString(from = STATIC_TAB, to = "STATIC_TAB"),
            @ViewDebug.IntToString(from = TAB_SWITCHER, to = "TAB_SWITCHER"),
            @ViewDebug.IntToString(from = ENTERING_TAB_SWITCHER, to = "ENTERING_TAB_SWITCHER"),
            @ViewDebug.IntToString(from = EXITING_TAB_SWITCHER, to = "EXITING_TAB_SWITCHER")
            })
    // clang-format on

    static final int LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 51;

    private TabCountProvider mTabCountProvider;

    protected LocationBarCoordinator mLocationBar;

    private ViewGroup mToolbarButtonsContainer;
    protected @Nullable ToggleTabStackButton mToggleTabStackButton;
    // Non-null after inflation occurs.
    protected @NonNull HomeButton mHomeButton;
    private TextView mUrlBar;
    protected View mUrlActionContainer;
    protected ImageView mToolbarShadow;
    private OptionalButtonCoordinator mOptionalButton;
    private boolean mOptionalButtonUsesTint;
    private boolean mShouldShowModernizeVisualUpdate;

    @ViewDebug.ExportedProperty(category = "chrome")
    protected int mTabSwitcherState;
    private boolean mForceHideShadow;

    // This determines whether or not the toolbar draws as expected (false) or whether it always
    // draws as if it's showing the non-tabswitcher, non-animating toolbar. This is used in grabbing
    // a bitmap to use as a texture representation of this view.
    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mTextureCaptureMode;
    private boolean mForceTextureCapture;

    private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private int mShowBrowserControlsToken = TokenHolder.INVALID_TOKEN;

    private TabSwitcherDrawable mTabSwitcherAnimationTabStackDrawable;

    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mUrlFocusChangeInProgress;

    /** 1.0 is 100% focused, 0 is completely unfocused */
    @ViewDebug.ExportedProperty(category = "chrome")
    private float mUrlFocusChangeFraction;

    /**
     * The degree to which the omnibox has expanded to full width, either because it is getting
     * focused or the NTP search box is being scrolled up. Note that in the latter case, the actual
     * width of the omnibox is not interpolated linearly from this value. The value will be the
     * maximum of {@link #mUrlFocusChangeFraction} and {@link #mNtpSearchBoxScrollFraction}.
     *
     * 0.0 == no expansion, 1.0 == fully expanded.
     */
    @ViewDebug.ExportedProperty(category = "chrome")
    protected float mUrlExpansionFraction;
    private AnimatorSet mUrlFocusLayoutAnimator;

    protected boolean mDisableLocationBarRelayout;
    protected boolean mLayoutLocationBarInFocusedMode;
    private boolean mLayoutLocationBarWithoutExtraButton;
    protected int mUnfocusedLocationBarLayoutWidth;
    protected int mUnfocusedLocationBarLayoutLeft;
    protected int mUnfocusedLocationBarLayoutRight;
    private boolean mUnfocusedLocationBarUsesTransparentBg;

    private int mLocationBarBackgroundAlpha = 255;
    private float mNtpSearchBoxScrollFraction = UNINITIALIZED_FRACTION;
    private float mStartSurfaceScrollFraction = UNINITIALIZED_FRACTION;
    protected ColorDrawable mToolbarBackground;

    /** The omnibox background (white with a shadow). */
    private GradientDrawable mLocationBarBackground;
    private Drawable mActiveLocationBarBackground;

    protected boolean mForceDrawLocationBarBackground;

    /** The boundaries of the omnibox, without the NTP-specific offset applied. */
    protected final Rect mLocationBarBackgroundBounds = new Rect();

    private final Rect mBackgroundOverlayBounds = new Rect();

    /** Offset applied to the bounds of the omnibox if we are showing a New Tab Page. */
    private final Rect mLocationBarBackgroundNtpOffset = new Rect();

    /**
     * Offsets applied to the <i>contents</i> of the omnibox if we are showing a New Tab Page.
     * This can be different from {@link #mLocationBarBackgroundNtpOffset} due to the fact that we
     * extend the omnibox horizontally beyond the screen boundaries when focused, to hide its
     * rounded corners.
     */
    private float mLocationBarNtpOffsetLeft;
    private float mLocationBarNtpOffsetRight;

    private final Rect mNtpSearchBoxBounds = new Rect();
    protected final Point mNtpSearchBoxTranslation = new Point();

    protected final int mToolbarSidePadding;
    private final int mBackgroundHeightIncreaseWhenFocus;

    private ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private boolean mIsHomeButtonEnabled;

    private Runnable mLayoutUpdater;

    /** The vertical inset of the location bar background. */
    private int mLocationBarBackgroundVerticalInset;

    /** The current color of the location bar. */
    private int mCurrentLocationBarColor;

    /** Whether the toolbar has a pending request to call {@link triggerUrlFocusAnimation()}. */
    private boolean mPendingTriggerUrlFocusRequest;
    private ToolbarSnapshotState mToolbarSnapshotState;
    private ButtonData mButtonData;
    /**
     * Whether the tab switcher is currently showing and controlled by the start surface. For
     * legacy reasons this bypasses the normal tab switcher state logic.
     * TODO(https://crbug.com/1315676): revisit the need for this once the tab switcher and start
     * surface are decoupled.
     */
    private boolean mIsShowingStartSurfaceTabSwitcher;

    /**
     * Used to specify the visual state of the toolbar.
     */
    @IntDef({VisualState.NORMAL, VisualState.INCOGNITO, VisualState.BRAND_COLOR,
            VisualState.NEW_TAB_NORMAL})
    @Retention(RetentionPolicy.SOURCE)
    @interface VisualState {
        int NORMAL = 0;
        int INCOGNITO = 1;
        int BRAND_COLOR = 2;
        int NEW_TAB_NORMAL = 3;
    }

    protected @VisualState int mVisualState = VisualState.NORMAL;

    private float mPreTextureCaptureAlpha = 1f;
    private int mPreTextureCaptureVisibility;
    private @BrandedColorScheme int mOverlayTabStackDrawableScheme;

    private boolean mOptionalButtonAnimationRunning;
    private int mUrlFocusTranslationX;

    private boolean mDropdownListScrolled;

    // The following are some properties used during animation.  We use explicit property classes
    // to avoid the cost of reflection for each animation setup.

    private final FloatProperty<ToolbarPhone> mUrlFocusChangeFractionProperty =
            new FloatProperty<ToolbarPhone>("") {
                @Override
                public Float get(ToolbarPhone object) {
                    return object.mUrlFocusChangeFraction;
                }

                @Override
                public void setValue(ToolbarPhone object, float value) {
                    setUrlFocusChangeFraction(value);
                }
            };

    /**
     * Constructs a ToolbarPhone object.
     *
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public ToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
        mShouldShowModernizeVisualUpdate = OmniboxFeatures.shouldShowModernizeVisualUpdate(context);
        final int edgePaddingRes = mShouldShowModernizeVisualUpdate
                ? R.dimen.toolbar_edge_padding_modern
                : R.dimen.toolbar_edge_padding;
        mToolbarSidePadding = getResources().getDimensionPixelOffset(edgePaddingRes);

        mBackgroundHeightIncreaseWhenFocus = mShouldShowModernizeVisualUpdate
                ? getResources().getDimensionPixelSize(
                        OmniboxFeatures.shouldShowActiveColorOnOmnibox()
                                ? R.dimen.toolbar_url_focus_height_increase_active_color
                                : R.dimen.toolbar_url_focus_height_increase_no_active_color)
                : 0;
    }

    @Override
    public void onFinishInflate() {
        try (TraceEvent te = TraceEvent.scoped("ToolbarPhone.onFinishInflate")) {
            super.onFinishInflate();
            mToolbarButtonsContainer = (ViewGroup) findViewById(R.id.toolbar_buttons);
            mHomeButton = findViewById(R.id.home_button);
            mUrlBar = (TextView) findViewById(R.id.url_bar);
            mUrlActionContainer = findViewById(R.id.url_action_container);
            mToolbarBackground =
                    new ColorDrawable(getToolbarColorForVisualState(VisualState.NORMAL));

            setLayoutTransition(null);

            if (getMenuButtonCoordinator() != null) {
                getMenuButtonCoordinator().setVisibility(true);
            }

            inflateTabSwitchingResources();

            setWillNotDraw(false);
            mUrlFocusTranslationX =
                    getResources().getDimensionPixelSize(R.dimen.toolbar_url_focus_translation_x);
        }
    }

    @Override
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {
        mLocationBar = locationBarCoordinator;
        initLocationBarBackground();
    }

    @Override
    public void setBrowserControlsVisibilityDelegate(
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate) {
        mBrowserControlsVisibilityDelegate = controlsVisibilityDelegate;
    }

    @Override
    public void destroy() {
        cancelAnimations();
        Handler handler = getHandler();
        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
        }

        super.destroy();
    }

    /**
     * Initializes the background, padding, margins, etc. for the location bar background.
     */
    private void initLocationBarBackground() {
        Resources res = getResources();
        mLocationBarBackgroundVerticalInset =
                res.getDimensionPixelSize(R.dimen.location_bar_vertical_margin);
        mLocationBarBackground = createModernLocationBarBackground(getContext());

        mActiveLocationBarBackground = mLocationBarBackground;
    }

    /**
     * @param context The activity {@link Context}.
     * @return The drawable for the modern location bar background.
     */
    public static GradientDrawable createModernLocationBarBackground(Context context) {
        GradientDrawable drawable = (GradientDrawable) context.getDrawable(
                R.drawable.modern_toolbar_text_box_background_with_primary_color);
        drawable.mutate();
        drawable.setTint(ChromeColors.getSurfaceColor(context, R.dimen.toolbar_text_box_elevation));

        return drawable;
    }

    /**
     * Set the background color of the location bar to appropriately match the theme color.
     */
    private void updateModernLocationBarColor(int color) {
        if (mCurrentLocationBarColor == color) return;
        mCurrentLocationBarColor = color;
        mLocationBarBackground.setTint(color);
        if (mOptionalButton != null) {
            mOptionalButton.setBackgroundColorFilter(color);
        }
    }

    private void updateModernLocationBarCorners() {
        if (!mShouldShowModernizeVisualUpdate
                || !OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            return;
        }
        int nonFocusedRadius = getResources().getDimensionPixelSize(
                R.dimen.modern_toolbar_background_corner_radius);
        int focusedRadius = getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_bg_round_corner_radius);
        int radius = (int) MathUtils.interpolate(
                nonFocusedRadius, focusedRadius, mUrlFocusChangeFraction);
        mLocationBarBackground.setCornerRadius(radius);
    }

    /**
     * Get the corresponding location bar color for a toolbar color.
     * @param toolbarColor The color of the toolbar.
     * @return The location bar color.
     */
    private int getLocationBarColorForToolbarColor(int toolbarColor) {
        return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                getContext(), toolbarColor, isIncognito());
    }

    /**
     * Get the toolbar default color depending on the toolbar's status.
     */
    private int getToolbarDefaultColor() {
        if (mShouldShowModernizeVisualUpdate && mLocationBar.getPhoneCoordinator().hasFocus()) {
            if (mDropdownListScrolled && !OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
                int colorRes = ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                        ? R.color.default_bg_color_dark_elev_2_gm3_baseline
                        : R.color.default_bg_color_dark_elev_2_baseline;
                return isIncognito() ? getContext().getColor(colorRes)
                                     : ChromeColors.getSurfaceColor(
                                             getContext(), R.dimen.toolbar_text_box_elevation);
            }
            return mLocationBar.getDropdownBackgroundColor(isIncognito());
        }
        return ChromeColors.getDefaultThemeColor(getContext(), isIncognito());
    }

    /**
     * Get the corresponding default location bar color for a toolbar color.
     * If location bar has focus, return the Omnibox suggestion background color.
     * If location bar does not have focus, return {@link getLocationBarColorForToolbarColor(int
     * toolbarColor)}.
     * @param toolbarColor The color of the toolbar.
     * @return The default location bar color.
     */
    private int getLocationBarDefaultColorForToolbarColor(int toolbarColor) {
        if (mShouldShowModernizeVisualUpdate && mLocationBar.getPhoneCoordinator().hasFocus()) {
            if (OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
                // Omnibox has same background as the Omnibox suggestion.
                return mLocationBar.getSuggestionBackgroundColor(isIncognito());
            }
            // Omnibox has same background as the toolbar.
            return getToolbarDefaultColor();
        }
        return getLocationBarColorForToolbarColor(toolbarColor);
    }

    private void inflateTabSwitchingResources() {
        mToggleTabStackButton = findViewById(R.id.tab_switcher_button);
        mToggleTabStackButton.setClickable(false);
    }

    private void enableTabSwitchingResources() {
        mToggleTabStackButton.setOnKeyListener(new KeyboardNavigationListener() {
            @Override
            public View getNextFocusForward() {
                if (isMenuButtonPresent()) {
                    return getMenuButtonCoordinator().getMenuButton();
                } else {
                    return getCurrentTabView();
                }
            }

            @Override
            public View getNextFocusBackward() {
                return findViewById(R.id.url_bar);
            }
        });
    }

    /**
     * Sets up click and key listeners once we have native library available to handle clicks.
     */
    @Override
    protected void onNativeLibraryReady() {
        super.onNativeLibraryReady();

        enableTabSwitchingResources();
        mHomeButton.setOnClickListener(this);

        getMenuButtonCoordinator().setOnKeyListener(new KeyboardNavigationListener() {
            @Override
            public View getNextFocusForward() {
                return getCurrentTabView();
            }

            @Override
            public View getNextFocusBackward() {
                return mToggleTabStackButton;
            }

            @Override
            protected boolean handleEnterKeyPress() {
                return getMenuButtonCoordinator().onEnterKeyPress();
            }
        });

        // Calls the {@link triggerUrlFocusAnimation()} here to finish the pending focus request if
        // it has been skipped in {@link onStartSurfaceStateChanged()}.
        if (mPendingTriggerUrlFocusRequest) {
            // This pending focus must be requested before native initialization when instant start
            // is enabled, whether user clicks omnibox or not.
            assert getToolbarDataProvider().isInOverviewAndShowingOmnibox();
            mPendingTriggerUrlFocusRequest = false;
            triggerUrlFocusAnimation(true);
        }

        updateVisualsForLocationBarState();
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // If the NTP is partially scrolled, prevent all touch events to the child views.  This
        // is to not allow a secondary touch event to trigger entering the tab switcher, which
        // can lead to really odd snapshots and transitions to the switcher.
        if (mNtpSearchBoxScrollFraction != 0f && mNtpSearchBoxScrollFraction != 1f
                && mNtpSearchBoxScrollFraction != UNINITIALIZED_FRACTION) {
            return true;
        }

        return super.onInterceptTouchEvent(ev);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Forward touch events to the NTP if the toolbar is moved away but the search box hasn't
        // reached the top of the page yet.
        if (mNtpSearchBoxTranslation.y < 0
                && mLocationBar.getPhoneCoordinator().getTranslationY() > 0) {
            return getToolbarDataProvider().getNewTabPageDelegate().dispatchTouchEvent(ev);
        }

        return super.onTouchEvent(ev);
    }

    @Override
    public void onClick(View v) {
        // Don't allow clicks while the omnibox is being focused.
        if (mLocationBar != null && mLocationBar.getPhoneCoordinator().hasFocus()) {
            return;
        }
        if (mHomeButton == v) {
            openHomepage();
            if (isNativeLibraryReady() && mPartnerHomepageEnabledSupplier.getAsBoolean()) {
                Profile profile = getToolbarDataProvider().getProfile();
                TrackerFactory.getTrackerForProfile(profile).notifyEvent(
                        EventConstants.PARTNER_HOME_PAGE_BUTTON_PRESSED);
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // In case the call came from the handler after we destroyed the dependencies, skip the work
        // that could touch the already destroyed objects.
        if (mDestroyChecker.isDestroyed()) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        if (!mDisableLocationBarRelayout) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);

            boolean changed = layoutLocationBarWithoutAnimationExpansion(
                    MeasureSpec.getSize(widthMeasureSpec));
            updateUrlExpansionAnimation();
            if (!changed) return;
        } else {
            updateUnfocusedLocationBarLayoutParams();
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * @return True if layout bar's unfocused width has changed, potentially causing updates to
     *         visual elements. If this happens during measurement pass, then toolbar's layout needs
     *         to be remeasured.
     */
    private boolean updateUnfocusedLocationBarLayoutParams() {
        int leftViewBounds = getViewBoundsLeftOfLocationBar(mVisualState);
        int rightViewBounds = getViewBoundsRightOfLocationBar(mVisualState);

        mUnfocusedLocationBarLayoutLeft = leftViewBounds;
        mUnfocusedLocationBarLayoutRight = rightViewBounds;
        int unfocusedLocationBarLayoutWidth = rightViewBounds - leftViewBounds;
        if (mUnfocusedLocationBarLayoutWidth != unfocusedLocationBarLayoutWidth) {
            mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
            mLocationBar.setUnfocusedWidth(mUnfocusedLocationBarLayoutWidth);
            return true;
        }
        return false;
    }

    /**
     * @return The background drawable for the toolbar view.
     */
    @VisibleForTesting
    public ColorDrawable getBackgroundDrawable() {
        return mToolbarBackground;
    }

    @VisibleForTesting
    void setLocationBarBackgroundDrawableForTesting(GradientDrawable background) {
        mLocationBarBackground = background;
    }

    @SuppressLint("RtlHardcoded")
    private boolean layoutLocationBar(int containerWidth) {
        TraceEvent.begin("ToolbarPhone.layoutLocationBar");

        boolean changed = layoutLocationBarWithoutAnimationExpansion(containerWidth);
        if (changed) updateLocationBarLayoutForExpansionAnimation();

        TraceEvent.end("ToolbarPhone.layoutLocationBar");

        return changed;
    }

    @SuppressLint("RtlHardcoded")
    private boolean layoutLocationBarWithoutAnimationExpansion(int containerWidth) {
        // Note that Toolbar's direction depends on system layout direction while
        // LocationBar's direction depends on its text inside.
        FrameLayout.LayoutParams locationBarLayoutParams =
                getFrameLayoutParams(getLocationBar().getContainerView());

        // Chrome prevents layout_gravity="left" from being defined in XML, but it simplifies
        // the logic, so it is manually specified here.
        locationBarLayoutParams.gravity = Gravity.TOP | Gravity.LEFT;

        int width = 0;
        int leftMargin = 0;

        // Always update the unfocused layout params regardless of whether we are using
        // those in this current layout pass as they are needed for animations.
        boolean changed = updateUnfocusedLocationBarLayoutParams();

        if (mLayoutLocationBarInFocusedMode
                || (mVisualState == VisualState.NEW_TAB_NORMAL
                        && mTabSwitcherState == STATIC_TAB)) {
            int priorVisibleWidth =
                    mLocationBar.getPhoneCoordinator().getOffsetOfFirstVisibleFocusedView();
            width = getFocusedLocationBarWidth(containerWidth, priorVisibleWidth);
            leftMargin = getFocusedLocationBarLeftMargin(priorVisibleWidth);
        } else {
            width = mUnfocusedLocationBarLayoutWidth;
            leftMargin = mUnfocusedLocationBarLayoutLeft;
        }

        if (mLayoutLocationBarWithoutExtraButton) {
            float offset = getLocationBarWidthOffsetForOptionalButton();
            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) leftMargin -= (int) offset;
            width += (int) offset;
        }

        changed |= (width != locationBarLayoutParams.width);
        locationBarLayoutParams.width = width;

        changed |= (leftMargin != locationBarLayoutParams.leftMargin);
        locationBarLayoutParams.leftMargin = leftMargin;

        return changed;
    }

    /**
     * @param containerWidth The width of the view containing the location bar.
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @return The width of the location bar when it has focus.
     */
    private int getFocusedLocationBarWidth(int containerWidth, int priorVisibleWidth) {
        int width = containerWidth - (2 * mToolbarSidePadding) + priorVisibleWidth;

        return width;
    }

    /**
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @return The left margin of the location bar when it has focus.
     */
    private int getFocusedLocationBarLeftMargin(int priorVisibleWidth) {
        int baseMargin = mToolbarSidePadding;
        if (mLocationBar.getPhoneCoordinator().getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            return baseMargin;
        } else {
            return baseMargin - priorVisibleWidth;
        }
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The left bounds of the location bar, accounting for any buttons on the left side
     *         of the toolbar.
     */
    private int getViewBoundsLeftOfLocationBar(@VisualState int visualState) {
        // Uses getMeasuredWidth()s instead of getLeft() because this is called in onMeasure
        // and the layout values have not yet been set.
        if (visualState == VisualState.NEW_TAB_NORMAL && mTabSwitcherState == STATIC_TAB) {
            return mToolbarSidePadding;
        } else if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            return getBoundsAfterAccountingForRightButtons();
        } else {
            return getBoundsAfterAccountingForLeftButton();
        }
    }

    /**
     * @return The left bounds of the location bar after accounting for any visible left buttons.
     */
    private int getBoundsAfterAccountingForLeftButton() {
        int padding = mToolbarSidePadding;

        // If home button is visible, or it's now in overview and toolbar is not shown (url bar
        // shouldn't be focused), mHomeButton.getMeasuredWidth() should be returned as the left
        // bound.
        if (mHomeButton.getVisibility() != GONE || isInOverviewAndToolbarInvisible()) {
            padding = mHomeButton.getMeasuredWidth();
        }
        return padding;
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The right bounds of the location bar, accounting for any buttons on the right side
     *         of the toolbar.
     */
    private int getViewBoundsRightOfLocationBar(@VisualState int visualState) {
        // Uses getMeasuredWidth()s instead of getRight() because this is called in onMeasure
        // and the layout values have not yet been set.
        if (visualState == VisualState.NEW_TAB_NORMAL && mTabSwitcherState == STATIC_TAB) {
            return getMeasuredWidth() - mToolbarSidePadding;
        } else if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            return getMeasuredWidth() - getBoundsAfterAccountingForLeftButton();
        } else {
            return getMeasuredWidth() - getBoundsAfterAccountingForRightButtons();
        }
    }

    /**
     * @return The right bounds of the location bar after accounting for any visible right buttons.
     */
    private int getBoundsAfterAccountingForRightButtons() {
        if (mStartSurfaceScrollFraction == 1.0f) return mToolbarSidePadding;

        int toolbarButtonsContainerWidth = mToolbarButtonsContainer.getMeasuredWidth();

        // MeasuredWidth() represents the desired width of the container which is accurate most
        // time, except during the optional button animations, where the MeasuredWidth changes
        // instantly to the final size and Width() represents the actual size at that frame.
        if (mOptionalButtonAnimationRunning) {
            toolbarButtonsContainerWidth = mToolbarButtonsContainer.getWidth();
        }

        return Math.max(mToolbarSidePadding, toolbarButtonsContainerWidth);
    }

    /**
     * Returns whether it's on overview mode (on start surface homepage or tab switcher surface) and
     * toolbar phone is not shown.
     */
    private boolean isInOverviewAndToolbarInvisible() {
        return getToolbarDataProvider().isInOverviewAndShowingOmnibox()
                && mStartSurfaceScrollFraction != 1.0f;
    }

    private void updateToolbarBackground(int color) {
        if (mToolbarBackground.getColor() == color) return;
        mToolbarBackground.setColor(color);
        setToolbarHairlineColor(color);
        invalidate();

        // We will set status bar's color same as the toolbar's color on phone form factor.
        notifyToolbarColorChanged(color);
    }

    private void updateToolbarBackgroundFromState(@VisualState int visualState) {
        updateToolbarBackground(getToolbarColorForVisualState(visualState));
    }

    private int getToolbarColorForVisualState(final @VisualState int visualState) {
        Resources res = getResources();
        switch (visualState) {
            case VisualState.NEW_TAB_NORMAL:
                // We are likely in the middle of a layout animation, and the NTP cannot draw itself
                // yet. Use the default background color, which will match what the NTP eventually
                // draws itself.
                if (!getToolbarDataProvider().getNewTabPageDelegate().hasCompletedFirstLayout()) {
                    return ChromeColors.getDefaultThemeColor(getContext(), false);
                }

                // When the NTP fake search box is visible, the background color should be
                // transparent. When the location bar reaches the top of the screen (i.e. location
                // bar is fully expanded), the background needs to change back to the default
                // toolbar color so that the NTP content is not visible beneath the toolbar. In
                // between the transition, we set a translucent default toolbar color based on
                // the expansion progress of the toolbar.
                return androidx.core.graphics.ColorUtils.setAlphaComponent(
                        ChromeColors.getDefaultThemeColor(getContext(), false),
                        Math.round(mUrlExpansionFraction * 255));
            case VisualState.NORMAL:
                return ChromeColors.getDefaultThemeColor(getContext(), false);
            case VisualState.INCOGNITO:
                return ChromeColors.getDefaultThemeColor(getContext(), true);
            case VisualState.BRAND_COLOR:
                return getToolbarDataProvider().getPrimaryColor();
            default:
                assert false;
                return SemanticColorUtils.getToolbarBackgroundPrimary(getContext());
        }
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (!mTextureCaptureMode && mToolbarBackground.getColor() != Color.TRANSPARENT) {
            // Update to compensate for orientation changes.
            mToolbarBackground.setBounds(0, 0, getWidth(), getHeight());
            mToolbarBackground.draw(canvas);
        }

        if (mLocationBarBackground != null
                && (mLocationBar.getPhoneCoordinator().getVisibility() == VISIBLE
                        || mTextureCaptureMode)) {
            updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, mVisualState);
        }

        if (mTextureCaptureMode) {
            draWithoutBackground(canvas);
        } else {
            super.dispatchDraw(canvas);
        }
    }

    @Override
    protected boolean verifyDrawable(Drawable who) {
        return super.verifyDrawable(who) || who == mActiveLocationBarBackground;
    }

    private void onNtpScrollChanged(float scrollFraction) {
        mNtpSearchBoxScrollFraction = scrollFraction;
        updateUrlExpansionFraction();
        updateUrlExpansionAnimation();
    }

    /**
     * @return True if the toolbar is showing tab switcher assets, including during transitions.
     */
    public boolean isInTabSwitcherMode() {
        return mTabSwitcherState != STATIC_TAB;
    }

    /**
     * Calculate the bounds for the location bar background and set them to {@code out}.
     */
    private void updateLocationBarBackgroundBounds(Rect out, @VisualState int visualState) {
        // Calculate the visible boundaries of the left and right most child views of the
        // location bar.
        float expansion = getExpansionFractionForVisualState(visualState);
        int leftViewPosition = getLeftPositionOfLocationBarBackground(visualState);
        int rightViewPosition = getRightPositionOfLocationBarBackground(visualState);
        int verticalInset = mLocationBarBackgroundVerticalInset - calculateOnFocusHeightIncrease();

        // The bounds are set by the following:
        // - The left most visible location bar child view.
        // - The top of the viewport is aligned with the top of the location bar.
        // - The right most visible location bar child view.
        // - The bottom of the viewport is aligned with the bottom of the location bar.
        // Additional padding can be applied for use during animations.
        out.set(leftViewPosition, mLocationBar.getPhoneCoordinator().getTop() + verticalInset,
                rightViewPosition, mLocationBar.getPhoneCoordinator().getBottom() - verticalInset);
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The left drawing position for the location bar background.
     */
    private int getLeftPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionFractionForVisualState(visualState);
        int leftViewPosition =
                (int) MathUtils.interpolate(getViewBoundsLeftOfLocationBar(visualState),
                        getFocusedLeftPositionOfLocationBarBackground(), expansion);

        return leftViewPosition;
    }

    /**
     * @return The left drawing position for the location bar background when the location bar
     *         has focus.
     */
    private int getFocusedLeftPositionOfLocationBarBackground() {
        return mToolbarSidePadding;
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The right drawing position for the location bar background.
     */
    private int getRightPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionFractionForVisualState(visualState);
        int rightViewPosition =
                (int) MathUtils.interpolate(getViewBoundsRightOfLocationBar(visualState),
                        getFocusedRightPositionOfLocationBarBackground(), expansion);

        return rightViewPosition;
    }

    /**
     * @return The difference in the location bar width when the optional button is hidden
     *         rather than showing. This is effectively the width of the optional button with
     *         some adjustment to account for possible padding differences when the button
     *         visibility changes.
     */
    @VisibleForTesting
    float getLocationBarWidthOffsetForOptionalButton() {
        float widthChange = mOptionalButton.getViewWidth();

        // When the optional button is the only visible button after the location bar and the
        // button is hidden mToolbarSidePadding is used for the padding after the location bar.
        if (!isMenuButtonPresent()) {
            widthChange -= mToolbarSidePadding;
        }
        return widthChange;
    }

    /**
     * @return The right drawing position for the location bar background when the location bar
     *         has focus.
     */
    private int getFocusedRightPositionOfLocationBarBackground() {
        return getWidth() - mToolbarSidePadding;
    }

    private float getExpansionFractionForVisualState(@VisualState int visualState) {
        return visualState == VisualState.NEW_TAB_NORMAL && mTabSwitcherState == STATIC_TAB
                ? 1
                : mUrlExpansionFraction;
    }

    /**
     * Updates progress of current the URL focus change animation.
     *
     * @param fraction 1.0 is 100% focused, 0 is completely unfocused.
     */
    private void setUrlFocusChangeFraction(float fraction) {
        mUrlFocusChangeFraction = fraction;
        updateUrlExpansionFraction();
        updateUrlExpansionAnimation();
    }

    private void updateUrlExpansionFraction() {
        mUrlExpansionFraction =
                Math.max(Math.max(mNtpSearchBoxScrollFraction, mStartSurfaceScrollFraction),
                        mUrlFocusChangeFraction);
        for (UrlExpansionObserver observer : mUrlExpansionObservers) {
            observer.onUrlExpansionProgressChanged(mUrlExpansionFraction);
        }
        assert mUrlExpansionFraction >= 0;
        assert mUrlExpansionFraction <= 1;
    }

    /**
     * Updates the parameters relating to expanding the location bar, as the result of either a
     * focus change or scrolling the New Tab Page.
     */
    private void updateUrlExpansionAnimation() {
        // TODO(https://crbug.com/865801): Prevent url expansion signals from happening while the
        // toolbar is not visible (e.g. in tab switcher mode).
        if (isInTabSwitcherMode()) return;

        int toolbarButtonVisibility = getToolbarButtonVisibility();
        mToolbarButtonsContainer.setVisibility(toolbarButtonVisibility);
        if (mHomeButton.getVisibility() != GONE) {
            mHomeButton.setVisibility(toolbarButtonVisibility);
        }

        updateLocationBarLayoutForExpansionAnimation();
    }

    /**
     * @return The visibility for {@link #mToolbarButtonsContainer}.
     */
    private int getToolbarButtonVisibility() {
        return (mUrlExpansionFraction == 1f
                       || getToolbarDataProvider().isInOverviewAndShowingOmnibox())
                ? INVISIBLE
                : VISIBLE;
    }

    /**
     * Updates the location bar layout, as the result of either a focus change or scrolling the
     * New Tab Page.
     */
    private void updateLocationBarLayoutForExpansionAnimation() {
        TraceEvent.begin("ToolbarPhone.updateLocationBarLayoutForExpansionAnimation");
        if (isInTabSwitcherMode()) return;

        boolean isLocationBarShownInNTP = isLocationBarShownInNTP();
        FrameLayout.LayoutParams locationBarLayoutParams =
                mLocationBar.getPhoneCoordinator().getFrameLayoutParams();
        int currentLeftMargin = locationBarLayoutParams.leftMargin;
        int currentWidth = locationBarLayoutParams.width;

        float locationBarBaseTranslationX = mUnfocusedLocationBarLayoutLeft - currentLeftMargin;
        if (mOptionalButtonAnimationRunning) {
            // When showing the button, we disable location bar relayout
            // (mDisableLocationBarRelayout), so the location bar's left margin and
            // mUnfocusedLocationBarLayoutLeft have not been updated to take into account the
            // appearance of the optional icon. The views to left of the location bar will
            // be wider than mUnfocusedlocationBarLayoutLeft in RTL, so adjust the translation by
            // that amount.
            // When hiding the button, we force a relayout without the optional toolbar button
            // (mLayoutLocationBarWithoutExtraButton). mUnfocusedLocationBarLayoutLeft reflects
            // the view bounds left of the location bar, which still includes the optional
            // button. The location bar left margin, however, has been adjusted to reflect its
            // end value when the optional button is fully hidden. The
            // locationBarBaseTranslationX above accounts for the difference between
            // mUnfocusedLocationBarLayoutLeft and the location bar's current left margin.
            locationBarBaseTranslationX +=
                    getViewBoundsLeftOfLocationBar(mVisualState) - mUnfocusedLocationBarLayoutLeft;
        }

        // When the dse icon is visible, the LocationBar needs additional translation to compensate
        // for the dse icon being laid out when focused. This also affects the UrlBar, which is
        // handled below. See comments in LocationBar#getLocationBarOffsetForFocusAnimation() for
        // implementation details.
        boolean isIncognito = getToolbarDataProvider().isIncognito();
        if (SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(isIncognito)) {
            locationBarBaseTranslationX += getLocationBarOffsetForFocusAnimation(hasFocus());
        }

        boolean isLocationBarRtl =
                mLocationBar.getPhoneCoordinator().getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if (isLocationBarRtl) {
            locationBarBaseTranslationX += mUnfocusedLocationBarLayoutWidth - currentWidth;
        }

        locationBarBaseTranslationX *= 1f - mUrlExpansionFraction;

        mLocationBarBackgroundNtpOffset.setEmpty();
        mLocationBarNtpOffsetLeft = 0;
        mLocationBarNtpOffsetRight = 0;

        Tab currentTab = getToolbarDataProvider().getTab();
        if (currentTab != null) {
            getToolbarDataProvider().getNewTabPageDelegate().setUrlFocusChangeAnimationPercent(
                    mUrlFocusChangeFraction);
            if (isLocationBarShownInNTP
                    && !getToolbarDataProvider().isInOverviewAndShowingOmnibox()) {
                updateNtpTransitionAnimation();
            } else {
                // Reset these values in case we transitioned to a different page during the
                // transition.
                resetNtpAnimationValues();
            }
        }

        float locationBarTranslationX;
        if (isLocationBarRtl) {
            locationBarTranslationX = locationBarBaseTranslationX + mLocationBarNtpOffsetRight;
        } else {
            locationBarTranslationX = locationBarBaseTranslationX + mLocationBarNtpOffsetLeft;
        }

        mLocationBar.getPhoneCoordinator().setTranslationX(locationBarTranslationX);

        if (!mOptionalButtonAnimationRunning) {
            mUrlActionContainer.setTranslationX(getUrlActionsTranslationXForExpansionAnimation(
                    isLocationBarRtl, locationBarBaseTranslationX));
            mLocationBar.setUrlFocusChangeFraction(mUrlExpansionFraction);

            // Only transition theme colors if in static tab mode that is not the NTP. In practice
            // this only runs when you focus the omnibox on a web page.
            // In NTP, toolbar and locationbar need to transite color only when the omnibox is
            // focused. When the fake omnibox is scrolled, the color should not change.
            if (((mShouldShowModernizeVisualUpdate && mLocationBar.getPhoneCoordinator().hasFocus())
                        || !isLocationBarShownInNTP)
                    && mTabSwitcherState == STATIC_TAB) {
                int defaultColor = getToolbarDefaultColor();
                int defaultLocationBarColor =
                        getLocationBarDefaultColorForToolbarColor(defaultColor);
                int primaryColor = getToolbarDataProvider().getPrimaryColor();
                int themedLocationBarColor = getLocationBarColorForToolbarColor(primaryColor);

                updateToolbarBackground(ColorUtils.getColorWithOverlay(
                        primaryColor, defaultColor, mUrlFocusChangeFraction));

                updateModernLocationBarColor(ColorUtils.getColorWithOverlay(
                        themedLocationBarColor, defaultLocationBarColor, mUrlFocusChangeFraction));

                updateModernLocationBarCorners();
                updateToolbarLayoutForUrlFocusChangeAnimation();
            }
        }

        // Force an invalidation of the location bar to properly handle the clipping of the URL
        // bar text as a result of the URL action container translations.
        mLocationBar.getPhoneCoordinator().invalidate();
        invalidate();
        TraceEvent.end("ToolbarPhone.updateLocationBarLayoutForExpansionAnimation");
    }

    /**
     * Updates the toolbar height and bottom padding during URL focus changing.
     */
    private void updateToolbarLayoutForUrlFocusChangeAnimation() {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(getContext())) {
            return;
        }

        int heightIncrease = calculateOnFocusHeightIncrease();

        var layoutParams = getLayoutParams();
        layoutParams.height = getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + heightIncrease;
        setLayoutParams(layoutParams);

        // Apply extra bottom padding.
        setPaddingRelative(getPaddingStart(), getPaddingTop(), getPaddingEnd(), heightIncrease);
    }

    /**
     * Calculates the translation X for the URL actions container for use in the URL expansion
     * animation.
     *
     * @param isLocationBarRtl Whether the location bar layout is RTL.
     * @param locationBarBaseTranslationX The base location bar translation for the URL expansion
     *                                    animation.
     * @return The translation X for the URL actions container.
     */
    private float getUrlActionsTranslationXForExpansionAnimation(
            boolean isLocationBarRtl, float locationBarBaseTranslationX) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        float urlActionsTranslationX = 0;
        if (!isLocationBarRtl || isRtl) {
            // Negate the location bar translation to keep the URL action container in the same
            // place during the focus expansion.
            urlActionsTranslationX = -locationBarBaseTranslationX;
        }

        if (isRtl) {
            urlActionsTranslationX += mLocationBarNtpOffsetLeft - mLocationBarNtpOffsetRight;
        } else {
            urlActionsTranslationX += mLocationBarNtpOffsetRight - mLocationBarNtpOffsetLeft;
        }

        return urlActionsTranslationX;
    }

    /**
     * Reset the parameters for the New Tab Page transition animation (expanding the location bar as
     * a result of scrolling the New Tab Page) to their default values.
     */
    private void resetNtpAnimationValues() {
        mLocationBarBackgroundNtpOffset.setEmpty();
        mActiveLocationBarBackground = mLocationBarBackground;
        mNtpSearchBoxTranslation.set(0, 0);
        mLocationBar.getPhoneCoordinator().setTranslationY(0);
        if (!mUrlFocusChangeInProgress) {
            mToolbarButtonsContainer.setTranslationY(0);
            mHomeButton.setTranslationY(0);
        }

        if (!mUrlFocusChangeInProgress && mToolbarShadow != null) {
            mToolbarShadow.setAlpha(mUrlBar.hasFocus() ? 0.f : 1.f);
        }

        mLocationBar.getPhoneCoordinator().setAlpha(1);
        mForceDrawLocationBarBackground = false;
        mLocationBarBackgroundAlpha = 255;
        if (isIncognito()
                || (mUnfocusedLocationBarUsesTransparentBg && !mUrlFocusChangeInProgress
                        && !mLocationBar.getPhoneCoordinator().hasFocus())) {
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        }

        setAncestorsShouldClipChildren(true);
        mNtpSearchBoxScrollFraction = UNINITIALIZED_FRACTION;
        updateUrlExpansionFraction();
    }

    /**
     * Updates the parameters of the New Tab Page transition animation (expanding the location bar
     * as a result of scrolling the New Tab Page).
     */
    private void updateNtpTransitionAnimation() {
        // Skip if in or entering tab switcher mode.
        if (mTabSwitcherState == TAB_SWITCHER || mTabSwitcherState == ENTERING_TAB_SWITCHER) return;

        boolean isExpanded = mUrlExpansionFraction > 0f;
        setAncestorsShouldClipChildren(!isExpanded);
        if (!mUrlFocusChangeInProgress) {
            float alpha = 0.f;
            if (!mUrlBar.hasFocus() && mNtpSearchBoxScrollFraction == 1.f) {
                alpha = 1.f;
            }
            mToolbarShadow.setAlpha(alpha);
        }

        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        // #getSearchBoxBounds is only valid once the NTP can actually draw itself.
        if (ntpDelegate.hasCompletedFirstLayout()) {
            ntpDelegate.getSearchBoxBounds(mNtpSearchBoxBounds, mNtpSearchBoxTranslation);
            int locationBarTranslationY = Math.max(
                    0, (mNtpSearchBoxBounds.top - mLocationBar.getPhoneCoordinator().getTop()));
            mLocationBar.getPhoneCoordinator().setTranslationY(locationBarTranslationY);

            updateButtonsTranslationY();

            // Linearly interpolate between the bounds of the search box on the NTP and the omnibox
            // background bounds. |shrinkage| is the scaling factor for the offset -- if it's 1, we
            // are shrinking the omnibox down to the size of the search box.
            float shrinkage = 1f
                    - Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR.getInterpolation(
                            mUrlExpansionFraction);

            int leftBoundDifference = mNtpSearchBoxBounds.left - mLocationBarBackgroundBounds.left;
            int rightBoundDifference =
                    mNtpSearchBoxBounds.right - mLocationBarBackgroundBounds.right;
            int verticalInset = (int) (getResources().getDimensionPixelSize(
                                               R.dimen.ntp_search_box_bounds_vertical_inset_modern)
                    * (1.f - mUrlExpansionFraction));
            mLocationBarBackgroundNtpOffset.set(Math.round(leftBoundDifference * shrinkage),
                    locationBarTranslationY, Math.round(rightBoundDifference * shrinkage),
                    locationBarTranslationY);
            mLocationBarBackgroundNtpOffset.inset(0, verticalInset);

            mLocationBarNtpOffsetLeft = leftBoundDifference * shrinkage;
            mLocationBarNtpOffsetRight = rightBoundDifference * shrinkage;
        }

        mLocationBarBackgroundAlpha = isExpanded ? 255 : 0;
        mForceDrawLocationBarBackground = mLocationBarBackgroundAlpha > 0;
        float relativeAlpha = mLocationBarBackgroundAlpha / 255f;
        mLocationBar.getPhoneCoordinator().setAlpha(relativeAlpha);

        // The search box on the NTP is visible if our omnibox is invisible, and vice-versa.
        ntpDelegate.setSearchBoxAlpha(1f - relativeAlpha);
        if (!mForceDrawLocationBarBackground) {
            if (mActiveLocationBarBackground instanceof NtpSearchBoxDrawable) {
                ((NtpSearchBoxDrawable) mActiveLocationBarBackground).resetBoundsToLastNonToolbar();
            }
        }

        updateToolbarBackgroundFromState(mVisualState);
    }

    /**
     * Update the y translation of the buttons to make it appear as if they were scrolling with
     * the new tab page.
     */
    private void updateButtonsTranslationY() {
        int transY = mTabSwitcherState == STATIC_TAB ? Math.min(mNtpSearchBoxTranslation.y, 0) : 0;

        mToolbarButtonsContainer.setTranslationY(transY);
        mHomeButton.setTranslationY(transY);
    }

    private void setAncestorsShouldClipChildren(boolean clip) {
        if (!isLocationBarShownInNTP()) return;

        ViewUtils.setAncestorsShouldClipChildren(this, clip);
    }

    /**
     * Draws all the browsing mode views at full alpha, but without a background.
     */
    @VisibleForTesting
    void draWithoutBackground(Canvas canvas) {
        if (!isNativeLibraryReady()) return;

        float floatAlpha = 1.0f;
        int rgbAlpha = 255;
        canvas.save();
        canvas.clipRect(mBackgroundOverlayBounds);

        float previousAlpha;
        if (mHomeButton.getVisibility() != View.GONE) {
            previousAlpha = mHomeButton.getAlpha();
            mHomeButton.setAlpha(previousAlpha * floatAlpha);
            drawChild(canvas, mHomeButton, SystemClock.uptimeMillis());
            mHomeButton.setAlpha(previousAlpha);
        }

        // Draw the location/URL bar.
        previousAlpha = mLocationBar.getPhoneCoordinator().getAlpha();
        mLocationBar.getPhoneCoordinator().setAlpha(previousAlpha * floatAlpha);
        // If the location bar is now fully transparent, do not bother drawing it.
        if (mLocationBar.getPhoneCoordinator().getAlpha() != 0 && isLocationBarCurrentlyShown()) {
            drawLocationBar(canvas, SystemClock.uptimeMillis());
        }
        mLocationBar.getPhoneCoordinator().setAlpha(previousAlpha);

        // Translate to draw end toolbar buttons.
        ViewUtils.translateCanvasToView(this, mToolbarButtonsContainer, canvas);

        if (mOptionalButton != null && mOptionalButton.getViewVisibility() != View.GONE) {
            canvas.save();
            ViewUtils.translateCanvasToView(
                    mToolbarButtonsContainer, mOptionalButton.getViewForDrawing(), canvas);
            mOptionalButton.getViewForDrawing().draw(canvas);
            canvas.restore();
        }

        // Draw the tab stack button and associated text if necessary.
        if (mTabSwitcherAnimationTabStackDrawable != null && mToggleTabStackButton != null
                && mUrlExpansionFraction != 1f) {
            // Draw the tab stack button image.
            canvas.save();
            ViewUtils.translateCanvasToView(
                    mToolbarButtonsContainer, mToggleTabStackButton, canvas);

            int backgroundWidth = mToggleTabStackButton.getDrawable().getIntrinsicWidth();
            int backgroundHeight = mToggleTabStackButton.getDrawable().getIntrinsicHeight();
            int backgroundLeft =
                    (mToggleTabStackButton.getWidth() - mToggleTabStackButton.getPaddingLeft()
                            - mToggleTabStackButton.getPaddingRight() - backgroundWidth)
                    / 2;
            backgroundLeft += mToggleTabStackButton.getPaddingLeft();
            int backgroundTop =
                    (mToggleTabStackButton.getHeight() - mToggleTabStackButton.getPaddingTop()
                            - mToggleTabStackButton.getPaddingBottom() - backgroundHeight)
                    / 2;
            backgroundTop += mToggleTabStackButton.getPaddingTop();
            canvas.translate(backgroundLeft, backgroundTop);

            mTabSwitcherAnimationTabStackDrawable.setBounds(
                    mToggleTabStackButton.getDrawable().getBounds());
            mTabSwitcherAnimationTabStackDrawable.setAlpha(rgbAlpha);
            mTabSwitcherAnimationTabStackDrawable.draw(canvas);
            canvas.restore();
        }

        // Draw the menu button if necessary.
        final MenuButtonCoordinator menuButtonCoordinator = getMenuButtonCoordinator();
        if (menuButtonCoordinator != null) {
            menuButtonCoordinator.drawTabSwitcherAnimationOverlay(
                    mToolbarButtonsContainer, canvas, rgbAlpha);
        }

        canvas.restore();
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        if (mLocationBar != null
                && child == mLocationBar.getPhoneCoordinator().getViewForDrawing()) {
            return drawLocationBar(canvas, drawingTime);
        }
        boolean clipped = false;

        if (mLocationBarBackground != null) {
            canvas.save();

            int translationY = (int) mLocationBar.getPhoneCoordinator().getTranslationY();
            int clipTop = mLocationBarBackgroundBounds.top + translationY;
            if (mUrlExpansionFraction != 0f && clipTop < child.getBottom()) {
                // For other child views, use the inverse clipping of the URL viewport.
                // Only necessary during animations.
                // Hardware mode does not support unioned clip regions, so clip using the
                // appropriate bounds based on whether the child is to the left or right of the
                // location bar.
                boolean isLeft = isChildLeft(child);

                int clipBottom = mLocationBarBackgroundBounds.bottom + translationY;
                boolean verticalClip = false;
                if (translationY > 0f) {
                    clipTop = child.getTop();
                    clipBottom = clipTop;
                    verticalClip = true;
                }

                if (isLeft) {
                    int clipRight = verticalClip ? child.getMeasuredWidth()
                                                 : mLocationBarBackgroundBounds.left;
                    canvas.clipRect(0, clipTop, clipRight, clipBottom);
                } else {
                    int clipLeft = verticalClip ? 0 : mLocationBarBackgroundBounds.right;
                    canvas.clipRect(clipLeft, clipTop, getMeasuredWidth(), clipBottom);
                }
            }
            clipped = true;
        }
        boolean retVal = super.drawChild(canvas, child, drawingTime);
        if (clipped) canvas.restore();
        return retVal;
    }

    private boolean isChildLeft(View child) {
        return child == mHomeButton ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * @return Whether or not the location bar should be drawing at any particular state of the
     *         toolbar.
     */
    private boolean shouldDrawLocationBar() {
        return mLocationBarBackground != null
                && (mTabSwitcherState == STATIC_TAB || mTextureCaptureMode);
    }

    private boolean drawLocationBar(Canvas canvas, long drawingTime) {
        TraceEvent.begin("ToolbarPhone.drawLocationBar");
        boolean clipped = false;
        if (shouldDrawLocationBar()) {
            canvas.save();

            if (shouldDrawLocationBarBackground()) {
                if (mActiveLocationBarBackground instanceof NtpSearchBoxDrawable) {
                    ((NtpSearchBoxDrawable) mActiveLocationBarBackground)
                            .markPendingBoundsUpdateFromToolbar();
                }
                mActiveLocationBarBackground.setBounds(
                        mLocationBarBackgroundBounds.left + mLocationBarBackgroundNtpOffset.left,
                        mLocationBarBackgroundBounds.top + mLocationBarBackgroundNtpOffset.top,
                        mLocationBarBackgroundBounds.right + mLocationBarBackgroundNtpOffset.right,
                        mLocationBarBackgroundBounds.bottom
                                + mLocationBarBackgroundNtpOffset.bottom);
                mActiveLocationBarBackground.draw(canvas);
            }

            float locationBarClipLeft =
                    mLocationBarBackgroundBounds.left + mLocationBarBackgroundNtpOffset.left;
            float locationBarClipRight =
                    mLocationBarBackgroundBounds.right + mLocationBarBackgroundNtpOffset.right;
            float locationBarClipTop =
                    mLocationBarBackgroundBounds.top + mLocationBarBackgroundNtpOffset.top;
            float locationBarClipBottom =
                    mLocationBarBackgroundBounds.bottom + mLocationBarBackgroundNtpOffset.bottom;

            final int locationBarPaddingStart =
                    mLocationBar.getPhoneCoordinator().getPaddingStart();
            final int locationBarPaddingEnd = mLocationBar.getPhoneCoordinator().getPaddingEnd();
            final int locationBarDirection =
                    mLocationBar.getPhoneCoordinator().getLayoutDirection();
            // When unexpanded, the location bar's visible content boundaries are inset from the
            // viewport used to draw the background.  During expansion transitions, compensation
            // is applied to increase the clip regions such that when the location bar converts
            // to the narrower collapsed layout the visible content is the same.
            if (mUrlExpansionFraction != 1f && !mOptionalButtonAnimationRunning) {
                int leftDelta = mUnfocusedLocationBarLayoutLeft
                        - getViewBoundsLeftOfLocationBar(mVisualState);
                int rightDelta = getViewBoundsRightOfLocationBar(mVisualState)
                        - mUnfocusedLocationBarLayoutLeft - mUnfocusedLocationBarLayoutWidth;
                float remainingFraction = 1f - mUrlExpansionFraction;

                locationBarClipLeft += leftDelta * remainingFraction;
                locationBarClipRight -= rightDelta * remainingFraction;

                // When the defocus animation is running, the location bar padding needs to be
                // subtracted from the clip bounds so that the location bar text width in the last
                // frame of the animation matches the text width of the unfocused location bar.
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipLeft += locationBarPaddingStart * remainingFraction;
                } else {
                    locationBarClipRight -= locationBarPaddingEnd * remainingFraction;
                }
            }
            if (mOptionalButtonAnimationRunning) {
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipLeft += locationBarPaddingStart;
                } else {
                    locationBarClipRight -= locationBarPaddingEnd;
                }
            }

            // Offset the clip rect by a set amount to ensure the Google G is completely inside the
            // omnibox background when animating in.
            if (SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(isIncognito())
                    && isLocationBarShownInNTP() && urlHasFocus() && mUrlFocusChangeInProgress) {
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipRight -= locationBarPaddingStart;
                } else {
                    locationBarClipLeft += locationBarPaddingStart;
                }
            }

            // Clip the location bar child to the URL viewport calculated in onDraw.
            canvas.clipRect(locationBarClipLeft, locationBarClipTop, locationBarClipRight,
                    locationBarClipBottom);
            clipped = true;
        }

        // TODO(1133482): Hide this View interaction if possible.
        boolean retVal = super.drawChild(
                canvas, mLocationBar.getPhoneCoordinator().getViewForDrawing(), drawingTime);

        if (clipped) canvas.restore();
        TraceEvent.end("ToolbarPhone.drawLocationBar");
        return retVal;
    }

    /**
     * @return Whether the location bar background should be drawn in
     *         {@link #drawLocationBar(Canvas, long)}.
     */
    private boolean shouldDrawLocationBarBackground() {
        return (mLocationBar.getPhoneCoordinator().getAlpha() > 0
                       || mForceDrawLocationBarBackground)
                && !mTextureCaptureMode;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mBackgroundOverlayBounds.set(0, 0, w, h);
        super.onSizeChanged(w, h, oldw, oldh);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mToolbarShadow = (ImageView) getRootView().findViewById(R.id.toolbar_hairline);
        updateShadowVisibility();
    }

    @Override
    public void draw(Canvas canvas) {
        if (mDestroyChecker.isDestroyed()) return;
        // If capturing a texture of the toolbar, ensure the alpha is set prior to draw(...) being
        // called.  The alpha is being used prior to getting to draw(...), so updating the value
        // after this point was having no affect.
        assert !mTextureCaptureMode || getAlpha() == 1f;
        super.draw(canvas);
    }

    @Override
    public void onStateRestored() {
        if (mToggleTabStackButton != null) mToggleTabStackButton.setClickable(true);
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        if (ToolbarFeatures.shouldBlockCapturesForAblation()) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SCROLL_ABLATION);
        } else if (mForceTextureCapture) {
            return CaptureReadinessResult.readyForced();
        } else if (ToolbarFeatures.shouldSuppressCaptures()) {
            return getReadinessStateWithSuppression();
        } else {
            return CaptureReadinessResult.unknown(!(urlHasFocus() || mUrlFocusChangeInProgress));
        }
    }

    private CaptureReadinessResult getReadinessStateWithSuppression() {
        if (urlHasFocus()) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS);
        } else if (mUrlFocusChangeInProgress) {
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.URL_BAR_FOCUS_IN_PROGRESS);
        } else if (mOptionalButtonAnimationRunning) {
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS);
        } else if (mLocationBar.getStatusCoordinator() != null
                && mLocationBar.getStatusCoordinator().isStatusIconAnimating()) {
            // TODO(https://crbug.com/1356153): It may be possible to remove the above null check.
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.STATUS_ICON_ANIMATION_IN_PROGRESS);
        } else if (isInTabSwitcherMode() || mIsShowingStartSurfaceTabSwitcher) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE);
        } else {
            ToolbarSnapshotState newSnapshotState = generateToolbarSnapshotState();
            @ToolbarSnapshotDifference
            int snapshotDifference = newSnapshotState.getAnyDifference(mToolbarSnapshotState);
            if (snapshotDifference == ToolbarSnapshotDifference.NONE) {
                return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SNAPSHOT_SAME);
            } else {
                return CaptureReadinessResult.readyWithSnapshotDifference(snapshotDifference);
            }
        }
    }

    @Override
    public boolean setForceTextureCapture(boolean forceTextureCapture) {
        if (forceTextureCapture) {
            // Only force a texture capture if the tint for the toolbar drawables is changing or
            // if the tab count has changed since the last texture capture.
            if (mToolbarSnapshotState == null) {
                mToolbarSnapshotState = generateToolbarSnapshotState();
            }

            mForceTextureCapture = mToolbarSnapshotState.getTint() != getTint().getDefaultColor();

            if (mTabSwitcherAnimationTabStackDrawable != null && mToggleTabStackButton != null) {
                mForceTextureCapture = mForceTextureCapture
                        || mToolbarSnapshotState.getTabCount()
                                != mTabSwitcherAnimationTabStackDrawable.getTabCount();
            }

            return mForceTextureCapture;
        }

        mForceTextureCapture = false;
        return false;
    }

    private ToolbarSnapshotState generateToolbarSnapshotState() {
        UrlBarData urlBarData;
        int securityIconResource;
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            urlBarData = mLocationBar.getUrlBarData();
            if (urlBarData == null) urlBarData = getToolbarDataProvider().getUrlBarData();
            StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
            securityIconResource = statusCoordinator == null
                    ? getToolbarDataProvider().getSecurityIconResource(false)
                    : statusCoordinator.getSecurityIconResource();
        } else {
            urlBarData = getToolbarDataProvider().getUrlBarData();
            securityIconResource = getToolbarDataProvider().getSecurityIconResource(false);
        }

        String displayedUrlText = urlBarData.displayText.toString();
        CharSequence prefixHint = mLocationBar.getOmniboxVisibleTextPrefixHint();
        boolean isValidPrefixHint =
                ToolbarSnapshotState.isValidVisibleTextPrefixHint(displayedUrlText, prefixHint);
        return new ToolbarSnapshotState(getTint().getDefaultColor(),
                mTabCountProvider.getTabCount(), mButtonData, mVisualState, displayedUrlText,
                isValidPrefixHint ? prefixHint : null, securityIconResource,
                ImageViewCompat.getImageTintList(mHomeButton),
                getMenuButtonCoordinator().isShowingUpdateBadge(),
                getToolbarDataProvider().isPaintPreview(), getProgressBar().getProgress(),
                mUnfocusedLocationBarLayoutWidth);
    }

    @Override
    public void setLayoutUpdater(Runnable layoutUpdater) {
        mLayoutUpdater = layoutUpdater;
    }

    @Override
    public void finishAnimations() {
        // The Android framework calls onAnimationEnd() on listeners before Animator#isRunning()
        // returns false. Sometimes this causes the progress bar visibility to be set incorrectly.
        // Update the visibility now that animations are set to null. (see crbug.com/606419)
        updateProgressBarVisibility();
    }

    @Override
    public void getLocationBarContentRect(Rect outRect) {
        updateLocationBarBackgroundBounds(outRect, VisualState.NORMAL);
    }

    @Override
    public void onHomeButtonUpdate(boolean homeButtonEnabled) {
        mIsHomeButtonEnabled = homeButtonEnabled;
        updateButtonVisibility();
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        updateButtonVisibility();
    }

    @Override
    public void updateButtonVisibility() {
        boolean hideHomeButton =
                !mIsHomeButtonEnabled || getToolbarDataProvider().isInOverviewAndShowingOmnibox();
        if (hideHomeButton) {
            removeHomeButton();
        } else {
            addHomeButton();
        }
        if (mOptionalButton != null) {
            if (isMenuButtonPresent()) {
                int padding = getResources().getDimensionPixelSize(
                        R.dimen.toolbar_phone_optional_button_padding);
                mOptionalButton.setPaddingStart(padding);
            } else {
                mOptionalButton.setPaddingStart(0);
            }
        }
    }

    @Override
    public void onTintChanged(ColorStateList tint, @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mHomeButton, tint);

        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setBrandedColorScheme(brandedColorScheme);
            if (mTabSwitcherAnimationTabStackDrawable != null) {
                mTabSwitcherAnimationTabStackDrawable.setTint(tint);
            }
        }

        if (mOptionalButton != null && mOptionalButtonUsesTint) {
            mOptionalButton.setIconForegroundColor(tint);
        }

        // TODO(amaralp): Have the LocationBar listen to tint changes.
        if (mLocationBar != null) mLocationBar.updateVisualsForState();

        if (mLayoutUpdater != null) mLayoutUpdater.run();
    }

    @Override
    public HomeButton getHomeButton() {
        return mHomeButton;
    }

    private void removeHomeButton() {
        mHomeButton.setVisibility(GONE);
    }

    private void addHomeButton() {
        mHomeButton.setVisibility(urlHasFocus() ? INVISIBLE : VISIBLE);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        assert mTextureCaptureMode != textureMode;
        mTextureCaptureMode = textureMode;
        if (mTextureCaptureMode) {
            if (!hideShadowForIncognitoNtp() && !hideShadowForInterstitial()
                    && !hideShadowForRegularNtpTextureCapture() && !mForceHideShadow) {
                mToolbarShadow.setVisibility(VISIBLE);
            }
            mPreTextureCaptureAlpha = getAlpha();
            mPreTextureCaptureVisibility = getVisibility();
            setAlpha(1);
            setVisibility(View.VISIBLE);
        } else {
            setAlpha(mPreTextureCaptureAlpha);
            setVisibility(mPreTextureCaptureVisibility);
            updateShadowVisibility();
            mPreTextureCaptureAlpha = 1f;

            // When texture mode is turned off, we know a capture has just been completed. Update
            // our snapshot so that we can suppress correctly on the next
            // #isReadyForTextureCapture() call.
            mToolbarSnapshotState = generateToolbarSnapshotState();
        }
    }

    private boolean hideShadowForRegularNtpTextureCapture() {
        return !isIncognito() && UrlUtilities.isNTPUrl(getToolbarDataProvider().getCurrentGurl())
                && mNtpSearchBoxScrollFraction < 1.f;
    }

    private void updateViewsForTabSwitcherMode() {
        setVisibility(mTabSwitcherState == TAB_SWITCHER ? View.INVISIBLE : View.VISIBLE);
        updateProgressBarVisibility();
        updateShadowVisibility();
        updateTabSwitcherButtonRipple();
    }

    private void updateProgressBarVisibility() {
        getProgressBar().setVisibility(
                (mTabSwitcherState != STATIC_TAB
                        || getToolbarDataProvider().isInOverviewAndShowingOmnibox()
                        || mIsShowingStartSurfaceTabSwitcher)
                        ? INVISIBLE
                        : VISIBLE);
    }

    @Override
    public void setContentAttached(boolean attached) {
        updateVisualsForLocationBarState();
    }

    @Override
    public void setTabSwitcherMode(boolean inTabSwitcherMode, boolean showToolbar,
            boolean delayAnimation, MenuButtonCoordinator menuButtonCoordinator) {
        // This method is only used for grid tab switcher with the start surface disabled. When
        // start surface is enabled, omnibox state is updated in onStartSurfaceStateChanged(), which
        // is always called before setTabSwitcherMode(), so skip here.
        if (getToolbarDataProvider().shouldShowLocationBarInOverviewMode()) {
            // Prevent pressing the tab switcher button until after transition finishes.
            mToggleTabStackButton.setClickable(false);
            return;
        }

        // If setting tab switcher mode to true and the browser is already animating or in the tab
        // switcher skip.
        if (inTabSwitcherMode
                && (mTabSwitcherState == TAB_SWITCHER
                        || mTabSwitcherState == ENTERING_TAB_SWITCHER)) {
            return;
        }

        // Likewise if exiting the tab switcher.
        if (!inTabSwitcherMode
                && (mTabSwitcherState == STATIC_TAB || mTabSwitcherState == EXITING_TAB_SWITCHER)) {
            return;
        }
        // Prevent pressing the tab switcher button until after transition finishes.
        mToggleTabStackButton.setClickable(false);

        mTabSwitcherState = inTabSwitcherMode ? ENTERING_TAB_SWITCHER : EXITING_TAB_SWITCHER;

        // The width of location bar depends on mTabSwitcherState so layout request is needed. See
        // crbug.com/974745.
        ViewUtils.requestLayout(this, "ToolbarPhone.setTabSwitcherMode");

        mLocationBar.setUrlBarFocusable(false);

        finishAnimations();

        if (inTabSwitcherMode) {
            if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
                mUrlFocusLayoutAnimator.end();
                mUrlFocusLayoutAnimator = null;
                // After finishing the animation, force a re-layout of the location bar,
                // so that the final translation position is correct (since onMeasure updates
                // won't happen in tab switcher mode). crbug.com/518795.
                layoutLocationBar(getMeasuredWidth());
            }

            updateViewsForTabSwitcherMode();
        }

        updateButtonsTranslationY();

        if (DeviceClassManager.enableAccessibilityLayout(getContext())) {
            finishAnimations();
        }

        postInvalidateOnAnimation();
    }

    /**
     * Enables or disables the tab switcher ripple depending on whether we are in or out of the tab
     * switcher mode.
     */
    private void updateTabSwitcherButtonRipple() {
        if (mToggleTabStackButton == null) return;
        if (mTabSwitcherState == ENTERING_TAB_SWITCHER || mIsShowingStartSurfaceTabSwitcher) {
            Drawable drawable = mToggleTabStackButton.getBackground();
            // drawable may not be a RippleDrawable if IPH is showing. Ignore that scenario since
            // it is rare.
            if (drawable instanceof RippleDrawable) {
                // Force the ripple to end so the transition looks correct.
                ((RippleDrawable) drawable).jumpToCurrentState();
            }
        }
    }

    @Override
    void onStartSurfaceStateChanged(boolean shouldBeVisible, boolean isShowingStartSurfaceHomepage,
            boolean isShowingStartSurfaceTabSwitcher) {
        if (mLocationBar == null || mLocationBar.isDestroyed()) return;
        super.onStartSurfaceStateChanged(
                shouldBeVisible, isShowingStartSurfaceHomepage, isShowingStartSurfaceTabSwitcher);

        mIsShowingStartSurfaceTabSwitcher = isShowingStartSurfaceTabSwitcher;
        // Update visibilities of toolbar layout, progress bar, shadow and ripple.
        setVisibility(shouldBeVisible ? VISIBLE : GONE);
        updateProgressBarVisibility();
        updateShadowVisibility();
        updateTabSwitcherButtonRipple();
        // Url bar should be focusable. This will be set in UrlBar#onDraw but there's a delay which
        // may cause focus to fail, so set here too.
        mLocationBar.setUrlBarFocusable(true);

        // Toolbar should be expanded when it's shown on the start surface homepage.
        float startSurfaceScrollFraction =
                shouldBeVisible && isShowingStartSurfaceHomepage ? 1.0f : 0.0f;
        if (mStartSurfaceScrollFraction != startSurfaceScrollFraction) {
            mStartSurfaceScrollFraction = startSurfaceScrollFraction;
            updateUrlExpansionFraction();
        }
        updateVisualsForLocationBarState();
    }

    @Override
    public void onTabSwitcherTransitionFinished() {
        setAlpha(1.f);

        // Detect what was being transitioned from and set the new state appropriately.
        if (mTabSwitcherState == EXITING_TAB_SWITCHER) {
            mLocationBar.setUrlBarFocusable(true);
            mTabSwitcherState = STATIC_TAB;
            updateVisualsForLocationBarState();
        }
        if (mTabSwitcherState == ENTERING_TAB_SWITCHER) {
            mTabSwitcherState = TAB_SWITCHER;
            mToggleTabStackButton.setClickable(false);
        }
        // Ensure clickable if in STATIC_TAB mode. This handles both when the state transitions
        // are ignored in favor of onStartSurfaceStateChanged() and when normal transitions occur.
        if (mTabSwitcherState == STATIC_TAB) {
            mToggleTabStackButton.setClickable(true);
        }

        // The width of location bar depends on mTabSwitcherState so layout request is needed. See
        // crbug.com/974745.
        ViewUtils.requestLayout(this, "ToolbarPhone.onTabSwitcherTransitionFinished");
        finishAnimations();
        updateVisualsForLocationBarState();
        updateViewsForTabSwitcherMode();
    }

    @Override
    public void setOnTabSwitcherClickHandler(OnClickListener listener) {
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setOnTabSwitcherClickHandler(listener);
        }
    }

    @Override
    void setOnTabSwitcherLongClickHandler(OnLongClickListener listener) {
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setOnTabSwitcherLongClickHandler(listener);
        }
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return super.shouldIgnoreSwipeGesture() || mUrlExpansionFraction > 0f
                || mNtpSearchBoxTranslation.y < 0f;
    }

    private void populateUrlExpansionAnimatorSet(List<Animator> animators) {
        TraceEvent.begin("ToolbarPhone.populateUrlFocusingAnimatorSet");
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangeFractionProperty, 1f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);

        mLocationBar.getPhoneCoordinator().populateFadeAnimation(
                animators, 0, URL_FOCUS_CHANGE_ANIMATION_DURATION_MS, 0);

        float density = getContext().getResources().getDisplayMetrics().density;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        float toolbarButtonTranslationX =
                MathUtils.flipSignIf(mUrlFocusTranslationX, isRtl) * density;

        animator = getMenuButtonCoordinator().getUrlFocusingAnimator(true);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        animators.add(animator);

        animator = ObjectAnimator.ofFloat(mHomeButton, TRANSLATION_X,
                MathUtils.flipSignIf(-mHomeButton.getWidth() * density, isRtl));
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        animators.add(animator);

        if (mToggleTabStackButton != null) {
            animator = ObjectAnimator.ofFloat(
                    mToggleTabStackButton, TRANSLATION_X, toolbarButtonTranslationX);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mToggleTabStackButton, ALPHA, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            animators.add(animator);
        }

        if (mToolbarShadow != null) {
            animator = ObjectAnimator.ofFloat(mToolbarShadow, ALPHA, urlHasFocus() ? 0 : 1);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }
        TraceEvent.end("ToolbarPhone.populateUrlFocusingAnimatorSet");
    }

    private void populateUrlClearExpansionAnimatorSet(List<Animator> animators) {
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangeFractionProperty, 0f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        animators.add(animator);

        animator = getMenuButtonCoordinator().getUrlFocusingAnimator(false);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        animators.add(animator);

        animator = ObjectAnimator.ofFloat(mHomeButton, TRANSLATION_X, 0);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        animators.add(animator);

        if (mToggleTabStackButton != null) {
            animator = ObjectAnimator.ofFloat(mToggleTabStackButton, TRANSLATION_X, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_TABSTACK_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);

            animator = ObjectAnimator.ofFloat(mToggleTabStackButton, ALPHA, 1);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_TABSTACK_DELAY_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }

        mLocationBar.getPhoneCoordinator().populateFadeAnimation(
                animators, URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS, URL_CLEAR_FOCUS_MENU_DELAY_MS, 1);

        if (isLocationBarShownInNTP() && mNtpSearchBoxScrollFraction == 0f) return;

        if (mToolbarShadow != null) {
            animator = ObjectAnimator.ofFloat(mToolbarShadow, ALPHA, 1);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
            animators.add(animator);
        }
    }

    @Override
    public void onUrlFocusChange(final boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);

        updateBackground(hasFocus);
        if (!hasFocus && mShouldShowModernizeVisualUpdate
                && !OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            mLocationBar.setStatusIconBackgroundVisibility(false);
        }

        if (mToggleTabStackButton != null) mToggleTabStackButton.setClickable(!hasFocus);
        triggerUrlFocusAnimation(hasFocus);
    }

    /**
     * @param hasFocus Whether the URL field has gained focus.
     */
    private void updateBackground(final boolean hasFocus) {
        if (hasFocus) {
            mDropdownListScrolled = false;
            mActiveLocationBarBackground = mLocationBarBackground;
        } else if (isLocationBarShownInNTP()) {
            updateToNtpBackground();
        }
    }

    private void updateToNtpBackground() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            NtpSearchBoxDrawable ntpSearchBox = new NtpSearchBoxDrawable(getContext(), this);
            getToolbarDataProvider().getNewTabPageDelegate().setSearchBoxBackground(ntpSearchBox);
            mActiveLocationBarBackground = ntpSearchBox;
        }
    }

    /**
     * @param hasFocus Whether the URL field has gained focus.
     */
    private void triggerUrlFocusAnimation(final boolean hasFocus) {
        boolean shouldShowKeyboard = urlHasFocus();

        TraceEvent.begin("ToolbarPhone.triggerUrlFocusAnimation");
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.cancel();
            mUrlFocusLayoutAnimator = null;
        }
        if (mOptionalButtonAnimationRunning) mOptionalButton.cancelTransition();

        List<Animator> animators = new ArrayList<>();
        if (hasFocus) {
            populateUrlExpansionAnimatorSet(animators);
        } else {
            populateUrlClearExpansionAnimatorSet(animators);
        }
        mUrlFocusLayoutAnimator = new AnimatorSet();
        mUrlFocusLayoutAnimator.playTogether(animators);

        mUrlFocusChangeInProgress = true;
        mUrlFocusLayoutAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                if (!hasFocus) {
                    mDisableLocationBarRelayout = true;
                } else {
                    mLayoutLocationBarInFocusedMode = true;
                    ViewUtils.requestLayout(ToolbarPhone.this,
                            "ToolbarPhone.triggerUrlFocusAnimation.CancelAwareAnimatorListener.onStart");
                }
            }

            @Override
            public void onCancel(Animator animation) {
                if (!hasFocus) mDisableLocationBarRelayout = false;

                mUrlFocusChangeInProgress = false;
            }

            @Override
            public void onEnd(Animator animation) {
                if (!hasFocus) {
                    mDisableLocationBarRelayout = false;
                    mLayoutLocationBarInFocusedMode = false;
                    ViewUtils.requestLayout(ToolbarPhone.this,
                            "ToolbarPhone.triggerUrlFocusAnimation.CancelAwareAnimatorListener.onEnd");
                }
                mLocationBar.finishUrlFocusChange(hasFocus, shouldShowKeyboard,
                        getToolbarDataProvider().shouldShowLocationBarInOverviewMode());
                mUrlFocusChangeInProgress = false;
            }
        });
        mUrlFocusLayoutAnimator.start();
        TraceEvent.end("ToolbarPhone.triggerUrlFocusAnimation");
    }

    @Override
    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        mTabCountProvider.addObserver(this);
        if (mToggleTabStackButton != null) {
            mToggleTabStackButton.setTabCountProvider(tabCountProvider);
        }
    }

    @Override
    public void onTabCountChanged(int numberOfTabs, boolean isIncognito) {
        mHomeButton.setEnabled(true);
        if (mToggleTabStackButton == null) return;

        @BrandedColorScheme
        int overlayTabStackDrawableScheme = OmniboxResourceProvider.getBrandedColorScheme(
                getContext(), isIncognito, getTabThemeColor());
        if (mTabSwitcherAnimationTabStackDrawable == null
                || mOverlayTabStackDrawableScheme != overlayTabStackDrawableScheme) {
            mTabSwitcherAnimationTabStackDrawable = TabSwitcherDrawable.createTabSwitcherDrawable(
                    getContext(), overlayTabStackDrawableScheme);
            int[] stateSet = {android.R.attr.state_enabled};
            mTabSwitcherAnimationTabStackDrawable.setState(stateSet);
            mOverlayTabStackDrawableScheme = overlayTabStackDrawableScheme;
        }

        if (mTabSwitcherAnimationTabStackDrawable != null) {
            mTabSwitcherAnimationTabStackDrawable.updateForTabCount(numberOfTabs, isIncognito);
        }
    }

    /**
     * Get the theme color for the currently active tab. This is not affected by the tab switcher's
     * theme color.
     * @return The current tab's theme color.
     */
    private int getTabThemeColor() {
        if (getToolbarDataProvider() != null) return getToolbarDataProvider().getPrimaryColor();
        return getToolbarColorForVisualState(
                isIncognito() ? VisualState.INCOGNITO : VisualState.NORMAL);
    }

    @Override
    public void onTabContentViewChanged() {
        super.onTabContentViewChanged();
        updateNtpAnimationState();
        updateVisualsForLocationBarState();
    }

    @Override
    public void onTabOrModelChanged() {
        super.onTabOrModelChanged();
        updateNtpAnimationState();
        updateVisualsForLocationBarState();
    }

    private static boolean isVisualStateValidForBrandColorTransition(@VisualState int state) {
        return state == VisualState.NORMAL || state == VisualState.BRAND_COLOR;
    }

    @Override
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        super.onPrimaryColorChanged(shouldAnimate);
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.end();

        final int initialColor = mToolbarBackground.getColor();
        final int finalColor = getToolbarDataProvider().getPrimaryColor();
        if (initialColor == finalColor) return;

        final int initialLocationBarColor = getLocationBarColorForToolbarColor(initialColor);
        final int finalLocationBarColor = getLocationBarColorForToolbarColor(finalColor);

        if (!isVisualStateValidForBrandColorTransition(mVisualState)) return;

        if (!shouldAnimate) {
            updateToolbarBackground(finalColor);
            return;
        }

        boolean shouldUseOpaque = ColorUtils.shouldUseOpaqueTextboxBackground(finalColor);
        final int initialAlpha = mLocationBarBackgroundAlpha;
        final int finalAlpha = shouldUseOpaque ? 255 : LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        final boolean shouldAnimateAlpha = initialAlpha != finalAlpha;
        mBrandColorTransitionAnimation =
                ValueAnimator.ofFloat(0, 1).setDuration(THEME_COLOR_TRANSITION_DURATION);
        mBrandColorTransitionAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        mBrandColorTransitionAnimation.addUpdateListener(new AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                float fraction = animation.getAnimatedFraction();
                if (shouldAnimateAlpha) {
                    mLocationBarBackgroundAlpha =
                            (int) (MathUtils.interpolate(initialAlpha, finalAlpha, fraction));
                }
                updateToolbarBackground(
                        ColorUtils.getColorWithOverlay(initialColor, finalColor, fraction));
                updateModernLocationBarColor(ColorUtils.getColorWithOverlay(
                        initialLocationBarColor, finalLocationBarColor, fraction));
            }
        });
        mBrandColorTransitionAnimation.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animation) {
                mBrandColorTransitionActive = false;
                updateVisualsForLocationBarState();
            }
        });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
        if (mLayoutUpdater != null) mLayoutUpdater.run();
    }

    private void updateNtpAnimationState() {
        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();

        // Store previous NTP scroll before calling reset as that clears this value.
        boolean wasShowingNtp = ntpDelegate.wasShowingNtp();
        float previousNtpScrollFraction = mNtpSearchBoxScrollFraction;

        resetNtpAnimationValues();
        ntpDelegate.setSearchBoxScrollListener(this::onNtpScrollChanged);
        if (ntpDelegate.isLocationBarShown()) {
            updateToNtpBackground();
            ViewUtils.requestLayout(
                    this, "ToolbarPhone.updateNtpAnimationState showing LocationBar");
        } else if (wasShowingNtp) {
            // Convert the previous NTP scroll progress to URL focus progress because that
            // will give a nicer transition animation from the expanded NTP omnibox to the
            // collapsed normal omnibox on other non-NTP pages.
            if (mTabSwitcherState == STATIC_TAB && previousNtpScrollFraction > 0f) {
                mUrlFocusChangeFraction =
                        Math.max(previousNtpScrollFraction, mUrlFocusChangeFraction);
                triggerUrlFocusAnimation(false);
            }
            ViewUtils.requestLayout(this, "ToolbarPhone.updateNtpAnimationState showing ntp");
        }
    }

    @Override
    public void onDefaultSearchEngineChanged() {
        super.onDefaultSearchEngineChanged();
        // Post an update for the toolbar state, which will allow all other listeners
        // for the search engine change to update before we check on the state of the
        // world for a UI update.
        // TODO(tedchoc): Move away from updating based on the search engine change and instead
        //                add the toolbar as a listener to the NewTabPage and udpate only when
        //                it notifies the listeners that it has changed its state.
        Handler handler = getHandler();
        if (handler == null) return;
        handler.post(() -> {
            updateVisualsForLocationBarState();
            updateNtpAnimationState();
        });
    }

    @Override
    public void handleFindLocationBarStateChange(boolean showing) {
        setVisibility(showing ? View.GONE
                              : mTabSwitcherState == STATIC_TAB ? View.VISIBLE : View.INVISIBLE);
    }

    private boolean isLocationBarShownInNTP() {
        return getToolbarDataProvider().getNewTabPageDelegate().isLocationBarShown();
    }

    private boolean isLocationBarCurrentlyShown() {
        return !isLocationBarShownInNTP() || mUrlExpansionFraction > 0;
    }

    @Override
    void setForceHideShadow(boolean forceHideShadow) {
        mForceHideShadow = forceHideShadow;
        updateShadowVisibility();
    }

    /**
     * Update the visibility of the toolbar shadow.
     */
    private void updateShadowVisibility() {
        boolean shouldDrawShadow = shouldDrawShadow();
        int shadowVisibility = shouldDrawShadow ? View.VISIBLE : View.INVISIBLE;

        if (mToolbarShadow != null && mToolbarShadow.getVisibility() != shadowVisibility) {
            mToolbarShadow.setVisibility(shadowVisibility);
        }
    }

    /**
     * @return Whether the toolbar shadow should be drawn.
     */
    private boolean shouldDrawShadow() {
        // TODO(twellington): Move this shadow state information to ToolbarDataProvider and show
        // shadow when incognito NTP is scrolled.
        return mTabSwitcherState == STATIC_TAB && !mForceHideShadow && !hideShadowForIncognitoNtp()
                && !hideShadowForInterstitial() && getVisibility() == View.VISIBLE
                && !mIsShowingStartSurfaceTabSwitcher;
    }

    private boolean hideShadowForIncognitoNtp() {
        return isIncognito() && UrlUtilities.isNTPUrl(getToolbarDataProvider().getCurrentGurl());
    }

    private boolean hideShadowForInterstitial() {
        return getToolbarDataProvider() != null && getToolbarDataProvider().getTab() != null
                && (getToolbarDataProvider().getTab().isShowingErrorPage());
    }

    private @VisualState int computeVisualState() {
        if (isLocationBarShownInNTP()) return VisualState.NEW_TAB_NORMAL;
        if (isIncognito()) return VisualState.INCOGNITO;
        if (getToolbarDataProvider().isUsingBrandColor()) return VisualState.BRAND_COLOR;
        return VisualState.NORMAL;
    }

    /**
     * @return The color that progress bar should use.
     */
    private int getProgressBarColor() {
        return getToolbarDataProvider().getPrimaryColor();
    }

    private void updateVisualsForLocationBarState() {
        TraceEvent.begin("ToolbarPhone.updateVisualsForLocationBarState");
        // These are used to skip setting state unnecessarily while in the tab switcher.
        boolean inOrEnteringStaticTab =
                mTabSwitcherState == STATIC_TAB || mTabSwitcherState == EXITING_TAB_SWITCHER;

        @VisualState
        int newVisualState = computeVisualState();

        if (newVisualState == VisualState.NEW_TAB_NORMAL && mHomeButton != null) {
            mHomeButton.setAccessibilityTraversalBefore(R.id.toolbar_buttons);
        } else {
            mHomeButton.setAccessibilityTraversalBefore(View.NO_ID);
        }

        // If we are navigating to or from a brand color, allow the transition animation
        // to run to completion as it will handle the triggering this path again and committing
        // the proper visual state when it finishes.  Brand color transitions are only valid
        // between normal non-incognito pages and brand color pages, so if the visual states
        // do not match then cancel the animation below.
        if (mBrandColorTransitionActive && isVisualStateValidForBrandColorTransition(mVisualState)
                && isVisualStateValidForBrandColorTransition(newVisualState)) {
            TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
            return;
        } else if (mBrandColorTransitionAnimation != null
                && mBrandColorTransitionAnimation.isRunning()) {
            mBrandColorTransitionAnimation.end();
        }

        boolean visualStateChanged = mVisualState != newVisualState;

        int currentPrimaryColor = getToolbarDataProvider().getPrimaryColor();
        int themeColorForProgressBar = getProgressBarColor();

        // If The page is native force the use of the standard theme for the progress bar.
        if (getToolbarDataProvider() != null && getToolbarDataProvider().getTab() != null
                && getToolbarDataProvider().getTab().isNativePage()) {
            @VisualState
            int visualState = isIncognito() ? VisualState.INCOGNITO : VisualState.NORMAL;
            themeColorForProgressBar = getToolbarColorForVisualState(visualState);
        }

        if (mVisualState == VisualState.BRAND_COLOR && !visualStateChanged) {
            boolean unfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            if (unfocusedLocationBarUsesTransparentBg != mUnfocusedLocationBarUsesTransparentBg) {
                visualStateChanged = true;
            } else {
                updateToolbarBackgroundFromState(VisualState.BRAND_COLOR);
                getProgressBar().setThemeColor(themeColorForProgressBar, isIncognito());
            }
        }

        mVisualState = newVisualState;

        // Refresh the toolbar texture.
        if ((mVisualState == VisualState.BRAND_COLOR || visualStateChanged)
                && mLayoutUpdater != null) {
            mLayoutUpdater.run();
        }
        updateShadowVisibility();
        updateUrlExpansionAnimation();

        // This exception is to prevent early change of theme color when exiting the tab switcher
        // since currently visual state does not map correctly to tab switcher state. See
        // https://crbug.com/832594 for more info.
        if (mTabSwitcherState != EXITING_TAB_SWITCHER) {
            updateToolbarBackgroundFromState(mVisualState);
        }

        if (!visualStateChanged) {
            if (mVisualState == VisualState.NEW_TAB_NORMAL) {
                updateNtpTransitionAnimation();
            } else {
                resetNtpAnimationValues();
            }
            TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
            return;
        }

        mUnfocusedLocationBarUsesTransparentBg = false;
        mLocationBarBackgroundAlpha = 255;
        getProgressBar().setThemeColor(themeColorForProgressBar, isIncognito());

        if (isIncognito()) {
            mLocationBarBackgroundAlpha = LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
        } else if (mVisualState == VisualState.BRAND_COLOR) {
            mUnfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            mLocationBarBackgroundAlpha = mUnfocusedLocationBarUsesTransparentBg
                    ? LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA
                    : 255;
        }

        updateModernLocationBarColor(getLocationBarColorForToolbarColor(currentPrimaryColor));

        mLocationBar.updateVisualsForState();

        // We update the alpha before comparing the visual state as we need to change
        // its value when entering and exiting TabSwitcher mode.
        if (isLocationBarShownInNTP() && inOrEnteringStaticTab) {
            updateNtpTransitionAnimation();
        }

        getMenuButtonCoordinator().setVisibility(true);
        TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    private void initializeOptionalButton() {
        if (mOptionalButton == null) {
            ViewStub optionalButtonStub = findViewById(R.id.optional_button_stub);

            if (optionalButtonStub == null) {
                return;
            }

            optionalButtonStub.setLayoutResource(R.layout.optional_button_layout);

            View optionalButton = optionalButtonStub.inflate();

            UserEducationHelper userEducationHelper =
                    new UserEducationHelper((Activity) getContext(), new Handler());

            BooleanSupplier isAnimationAllowedPredicate = new BooleanSupplier() {
                @Override
                public boolean getAsBoolean() {
                    boolean transitioningAwayFromLocationBarInNTP =
                            getToolbarDataProvider()
                                    .getNewTabPageDelegate()
                                    .transitioningAwayFromLocationBar();
                    boolean isInOverviewAndShowingOmnibox =
                            getToolbarDataProvider().isInOverviewAndShowingOmnibox();

                    return mTabSwitcherState == STATIC_TAB && !mUrlFocusChangeInProgress
                            && !urlHasFocus() && !transitioningAwayFromLocationBarInNTP
                            && !isInOverviewAndShowingOmnibox;
                }
            };

            Profile profile = Profile.getLastUsedRegularProfile();
            Tracker featureEngagementTracker = TrackerFactory.getTrackerForProfile(profile);
            mOptionalButton = new OptionalButtonCoordinator(optionalButton, userEducationHelper,
                    /* transitionRoot= */ mToolbarButtonsContainer, isAnimationAllowedPredicate,
                    featureEngagementTracker);

            // Set the button's background to the same color as the URL bar background. This color
            // is only used when showing dynamic actions.
            mOptionalButton.setBackgroundColorFilter(mCurrentLocationBarColor);
            mOptionalButton.setOnBeforeHideTransitionCallback(
                    () -> mLayoutLocationBarWithoutExtraButton = true);

            mOptionalButton.setTransitionStartedCallback(transitionType -> {
                mOptionalButtonAnimationRunning = true;
                keepControlsShownForAnimation();

                switch (transitionType) {
                    case TransitionType.COLLAPSING_ACTION_CHIP:
                    case TransitionType.HIDING:
                        mLayoutLocationBarWithoutExtraButton = true;
                        ViewUtils.requestLayout(this,
                                "ToolbarPhone.initializeOptionalButton.mOptionalButton.setTransitionStartedCallback");
                        break;
                    case TransitionType.EXPANDING_ACTION_CHIP:
                    case TransitionType.SHOWING:
                        mDisableLocationBarRelayout = true;
                        break;
                }
            });
            mOptionalButton.setTransitionFinishedCallback(transitionType -> {
                // If we are done expanding the transition chip then don't re-enable hiding browser
                // controls, as we'll begin the collapse transition soon.
                if (transitionType == TransitionType.EXPANDING_ACTION_CHIP) {
                    return;
                }

                mLayoutLocationBarWithoutExtraButton = false;
                mDisableLocationBarRelayout = false;
                mOptionalButtonAnimationRunning = false;
                allowBrowserControlsHide();
                if (mLayoutUpdater != null) mLayoutUpdater.run();
                ViewUtils.requestLayout(this,
                        "ToolbarPhone.initializeOptionalButton.mOptionalButton.setTransitionFinishedCallback");
            });
        }
    }

    @Override
    void updateOptionalButton(ButtonData buttonData) {
        mButtonData = buttonData;
        ButtonSpec buttonSpec = mButtonData.getButtonSpec();

        if (mOptionalButton == null) {
            initializeOptionalButton();
        }

        mOptionalButtonUsesTint = buttonSpec.getSupportsTinting();

        mOptionalButton.updateButton(buttonData);

        if (mOptionalButtonUsesTint) {
            mOptionalButton.setIconForegroundColor(getTint());
        } else {
            mOptionalButton.setIconForegroundColor(null);
        }
    }

    @Override
    void hideOptionalButton() {
        mButtonData = null;
        if (mOptionalButton == null || mOptionalButton.getViewVisibility() == View.GONE
                || mLayoutLocationBarWithoutExtraButton) {
            return;
        }

        mOptionalButton.hideButton();
    }

    @Override
    @VisibleForTesting
    public View getOptionalButtonViewForTesting() {
        if (mOptionalButton != null) {
            return mOptionalButton.getButtonViewForTesting();
        }

        return null;
    }

    /**
     * Whether the menu button is visible. Used as a proxy for whether there are end toolbar
     * buttons besides the optional button.
     */
    private boolean isMenuButtonPresent() {
        return getMenuButtonCoordinator().isVisible();
    }

    private void keepControlsShownForAnimation() {
        // isShown() being false implies that the toolbar isn't visible. We don't want to force it
        // back into visibility just so that we can show an animation.
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

    /**
     * Custom drawable that allows sharing the NTP search box drawable between the toolbar and the
     * NTP.  This allows animations to continue as the drawable is switched between the two owning
     * views.
     */
    private static class NtpSearchBoxDrawable extends DrawableWrapperCompat {
        private final Drawable.Callback mCallback;

        private int mBoundsLeft;
        private int mBoundsTop;
        private int mBoundsRight;
        private int mBoundsBottom;
        private boolean mPendingBoundsUpdateFromToolbar;
        private boolean mDrawnByNtp;

        /**
         * Constructs the NTP search box drawable.
         *
         * @param context The context used to inflate the drawable.
         * @param callback The callback to be notified on changes ot the drawable.
         */
        public NtpSearchBoxDrawable(Context context, Drawable.Callback callback) {
            super(context.getDrawable(R.drawable.ntp_search_box));

            mCallback = callback;
            setCallback(mCallback);
        }

        /**
         * Mark that the pending bounds update is coming from the toolbar.
         */
        void markPendingBoundsUpdateFromToolbar() {
            mPendingBoundsUpdateFromToolbar = true;
        }

        /**
         * Reset the bounds of the drawable to the last bounds received that was not marked from
         * the toolbar.
         */
        void resetBoundsToLastNonToolbar() {
            setBounds(mBoundsLeft, mBoundsTop, mBoundsRight, mBoundsBottom);
        }

        @Override
        public void setBounds(int left, int top, int right, int bottom) {
            super.setBounds(left, top, right, bottom);
            if (!mPendingBoundsUpdateFromToolbar) {
                mBoundsLeft = left;
                mBoundsTop = top;
                mBoundsRight = right;
                mBoundsBottom = bottom;
                mDrawnByNtp = true;
            } else {
                mDrawnByNtp = false;
            }
            mPendingBoundsUpdateFromToolbar = false;
        }

        @Override
        public boolean setVisible(boolean visible, boolean restart) {
            // Ignore visibility changes.  The NTP can toggle the visibility based on the scroll
            // position of the page, so we simply ignore all of this as we expect the drawable to
            // be visible at all times of the NTP.
            return false;
        }

        @Override
        public Callback getCallback() {
            return mDrawnByNtp ? super.getCallback() : mCallback;
        }
    }

    private void cancelAnimations() {
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.cancel();
        }

        if (mBrandColorTransitionAnimation != null && mBrandColorTransitionAnimation.isRunning()) {
            mBrandColorTransitionAnimation.cancel();
        }
    }

    /**
     * Calculates the offset required for the focused LocationBar to appear as if it's still
     * unfocused so it can animate to a focused state.
     *
     * @param hasFocus True if the LocationBar has focus, this will be true between the focus
     *                 animation starting and the unfocus animation starting.
     * @return The offset for the location bar when showing the dse icon.
     */
    private int getLocationBarOffsetForFocusAnimation(boolean hasFocus) {
        StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
        if (statusCoordinator == null) return 0;

        // No offset is required if the experiment is disabled.
        if (!SearchEngineLogoUtils.getInstance().shouldShowSearchEngineLogo(
                    getToolbarDataProvider().isIncognito())) {
            return 0;
        }

        // On non-NTP pages, there will always be an icon when unfocused.
        if (!getToolbarDataProvider().getNewTabPageDelegate().isCurrentlyVisible()) return 0;

        // This offset is only required when the focus animation is running.
        if (!hasFocus) return 0;

        // We're on the NTP with the fakebox showing.
        // The value returned changes based on if the layout is LTR OR RTL.
        // For LTR, the value is negative because we are making space on the left-hand side.
        // For RTL, the value is positive because we are pushing the icon further to the
        // right-hand side.
        int offset = statusCoordinator.getStatusIconWidth() - getAdditionalOffsetForNTP();
        return mLocationBar.isLayoutRtl() ? offset : -offset;
    }

    private int getAdditionalOffsetForNTP() {
        return getResources().getDimensionPixelSize(mShouldShowModernizeVisualUpdate
                               ? R.dimen.location_bar_start_padding_modern
                               : R.dimen.location_bar_start_padding)
                - getResources().getDimensionPixelSize(R.dimen.fake_search_box_lateral_padding);
    }

    @Override
    @VisibleForTesting
    public boolean isAnimationRunningForTesting() {
        return mUrlFocusChangeInProgress || mBrandColorTransitionActive
                || mOptionalButtonAnimationRunning;
    }

    /**
     * Returns the toolbar's background color.
     */
    public int getToolbarBackgroundColor() {
        return mToolbarBackground.getColor();
    }

    @Override
    public void onSuggestionDropdownScroll() {
        if (!mShouldShowModernizeVisualUpdate || OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            return;
        }
        mDropdownListScrolled = true;
        mLocationBar.setStatusIconBackgroundVisibility(true);
        updateToolbarAndLocationBarColor();
    }

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {
        if (!mShouldShowModernizeVisualUpdate || OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            return;
        }
        mDropdownListScrolled = false;
        mLocationBar.setStatusIconBackgroundVisibility(false);
        updateToolbarAndLocationBarColor();
    }

    private void updateToolbarAndLocationBarColor() {
        int toolbarDefaultColor = getToolbarDefaultColor();
        updateToolbarBackground(toolbarDefaultColor);
        updateModernLocationBarColor(
                getLocationBarDefaultColorForToolbarColor(toolbarDefaultColor));
    }

    private int calculateOnFocusHeightIncrease() {
        return (int) (mBackgroundHeightIncreaseWhenFocus * mUrlFocusChangeFraction / 2);
    }
}
