// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
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
import android.os.Handler;
import android.os.SystemClock;
import android.transition.ChangeBounds;
import android.transition.ChangeTransform;
import android.transition.Fade;
import android.transition.Slide;
import android.transition.Transition;
import android.transition.Transition.TransitionListener;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.transition.TransitionValues;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewDebug;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarBackgroundDrawable;
import org.chromium.chrome.browser.omnibox.LocationBarBackgroundDrawable.HairlineBehavior;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.back_button.BackButtonCoordinator;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.forward_button.ForwardButtonCoordinator;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator.TransitionType;
import org.chromium.chrome.browser.toolbar.reload_button.ReloadButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator.ToolbarColorObserver;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.KeyboardNavigationListener;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Phone specific toolbar implementation. */
@NullMarked
public class ToolbarPhone extends ToolbarLayout
        implements OmniboxSuggestionsDropdownScrollListener, ToolbarDataProvider.Observer {
    /** The amount of time transitioning from one theme color to another should take in ms. */
    public static final long THEME_COLOR_TRANSITION_DURATION = 250;

    public static final int URL_FOCUS_CHANGE_ANIMATION_DURATION_MS = 225;
    private static final int URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS = 100;
    private static final int URL_CLEAR_FOCUS_TABSTACK_DELAY_MS = 200;
    private static final int URL_CLEAR_FOCUS_MENU_DELAY_MS = 250;

    // Values used during animation to show/hide optional toolbar button.
    private static final float UNINITIALIZED_FRACTION = -1f;

    /** States that the toolbar can be in regarding the tab switcher. */
    protected static final int STATIC_TAB = 0;

    protected static final int TAB_SWITCHER = 1;
    protected static final int ENTERING_TAB_SWITCHER = 2;
    protected static final int EXITING_TAB_SWITCHER = 3;

    @ViewDebug.ExportedProperty(
            category = "chrome",
            mapping = {
                @ViewDebug.IntToString(from = STATIC_TAB, to = "STATIC_TAB"),
                @ViewDebug.IntToString(from = TAB_SWITCHER, to = "TAB_SWITCHER"),
                @ViewDebug.IntToString(from = ENTERING_TAB_SWITCHER, to = "ENTERING_TAB_SWITCHER"),
                @ViewDebug.IntToString(from = EXITING_TAB_SWITCHER, to = "EXITING_TAB_SWITCHER")
            })
    private @Nullable ObservableSupplier<Integer> mTabCountSupplier;

    private UserEducationHelper mUserEducationHelper;
    protected LocationBarCoordinator mLocationBar;
    private ObservableSupplier<Tracker> mTrackerSupplier;

    private ViewGroup mToolbarButtonsContainer;
    private @MonotonicNonNull OptionalButtonCoordinator mOptionalButtonCoordinator;
    private HomeButtonDisplay mHomeButtonDisplay;

    @ViewDebug.ExportedProperty(category = "chrome")
    protected int mTabSwitcherState;

    // This determines whether or not the toolbar draws as expected (false) or whether it always
    // draws as if it's showing the non-tabswitcher, non-animating toolbar. This is used in grabbing
    // a bitmap to use as a texture representation of this view.
    @ViewDebug.ExportedProperty(category = "chrome")
    protected boolean mTextureCaptureMode;

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
     * <p>0.0 == no expansion, 1.0 == fully expanded.
     */
    @ViewDebug.ExportedProperty(category = "chrome")
    protected float mUrlExpansionFraction;

    private @Nullable AnimatorSet mUrlFocusLayoutAnimator;

    protected boolean mDisableLocationBarRelayout;
    protected boolean mLayoutLocationBarInFocusedMode;
    private boolean mLayoutLocationBarWithoutExtraButton;
    protected int mUnfocusedLocationBarLayoutWidth;
    protected int mUnfocusedLocationBarLayoutLeft;
    protected int mUnfocusedLocationBarLayoutRight;
    private boolean mUnfocusedLocationBarUsesTransparentBg;

    private float mNtpSearchBoxScrollFraction = UNINITIALIZED_FRACTION;
    protected ColorDrawable mToolbarBackground;

    /** The omnibox background (white with a shadow). */
    private LocationBarBackgroundDrawable mLocationBarBackground;

    /**
     * The host for the background drawable when TOOLBAR_PHONE_ANIMATION_REFACTOR is enabled.
     * TODO(crbug.com/425817689): Remove the Drawable reference when the above refactor is complete
     * and we no longer need to manually draw the Drawable to the canvas.
     */
    private View mActiveLocationBarBackgroundView;

    private Drawable mActiveLocationBarBackground;
    private @Nullable GradientDrawable mNtpFakeboxBackground;

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

    /**
     * Offset applied to the URL actions container due to the end padding of the fake search box on
     * NTP.
     */
    private float mUrlActionsNtpEndOffset;

    /**
     * Offset applied to the location bar when scrolling during a refactored transition. This is to
     * avoid "double-counting" the offset that is already included in the animation bounds.
     */
    private float mRefactoredNtpStartingOffset;

    private final Rect mNtpSearchBoxBounds = new Rect();
    protected final Point mNtpSearchBoxTranslation = new Point();

    private final int mToolbarSidePadding;
    private final int mToolbarSidePaddingForNtp;
    private final int mBackgroundHeightIncreaseWhenFocus;
    private final int mVerticalPaddingWhenFocused;
    private int mTopPaddingForEdgeToEdgeNtp;

    private @Nullable ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private boolean mIsHomeButtonEnabled;
    private boolean mIsHomepageNonNtp;

    private @Nullable Runnable mLayoutUpdater;
    private @Nullable Runnable mDefaultSearchEngineChangedRunnable;

    /** The vertical inset of the location bar background. */
    private int mLocationBarBackgroundVerticalInset;

    /** The current color of the location bar. */
    private @ColorInt int mCurrentLocationBarColor;

    private @Nullable PhoneCaptureStateToken mPhoneCaptureStateToken;
    private @Nullable ButtonData mButtonData;

    private final @ColorInt int mToolbarBackgroundColorForNtp;
    private final @ColorInt int mLocationBarBackgroundColorForNtp;
    private final boolean mUseAdjustedTintColorForNtp;
    private boolean mIsToolbarExpandedOnNtp;

    /** Used to specify the visual state of the toolbar. */
    @IntDef({
        VisualState.NORMAL,
        VisualState.INCOGNITO,
        VisualState.BRAND_COLOR,
        VisualState.NEW_TAB_NORMAL,
        VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VisualState {
        int NORMAL = 0;
        int INCOGNITO = 1;
        int BRAND_COLOR = 2;
        int NEW_TAB_NORMAL = 3;
        int NEW_TAB_SEARCH_ENGINE_NO_LOGO = 4;
    }

    protected @VisualState int mVisualState = VisualState.NORMAL;

    private float mPreTextureCaptureAlpha = 1f;
    private int mPreTextureCaptureVisibility;

    private boolean mOptionalButtonAnimationRunning;
    private long mOptionalButtonAnimationStartTimeMs;
    private int mUrlFocusTranslationX;

    private boolean mDropdownListScrolled;

    // If we're in a layout transition, between startHiding to doneShowing.
    private boolean mInLayoutTransition;

    // For NTP, we have a different appearance (G logo background, search text color and style) of
    // the real search box after it's pinned at top when scrolling up the surface. This variable
    // distinguishes whether the current page is NTP with unfocused real omnibox, to indicate that
    // the appearance of the real search box changed.
    private boolean mIsNtpWithUnfocusedRealOmnibox;

    // Added due to https://crbug.com/323888159 to mark the loading phase while navigating from NTP
    // to webpages.
    private boolean mIsInLoadingPhaseFromNtpToWebpage;

    // The following are some properties used during animation.  We use explicit property classes
    // to avoid the cost of reflection for each animation setup.

    private final FloatProperty<ToolbarPhone> mUrlFocusChangeFractionProperty =
            new FloatProperty<>("") {
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
        mToolbarSidePadding = OmniboxResourceProvider.getToolbarSidePadding(context);
        mToolbarSidePaddingForNtp = OmniboxResourceProvider.getToolbarSidePaddingForNtp(context);
        mBackgroundHeightIncreaseWhenFocus =
                OmniboxResourceProvider.getLocationBarBackgroundOnFocusHeightIncrease(context);
        mVerticalPaddingWhenFocused =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen.toolbar_vertical_padding_when_bottom_focused);
        mToolbarBackgroundColorForNtp =
                ContextCompat.getColor(getContext(), R.color.home_surface_background_color);
        float locationBarBackgroundColorAlphaForNtp =
                ResourcesCompat.getFloat(
                        getResources(), R.dimen.home_surface_search_box_background_alpha);
        mLocationBarBackgroundColorForNtp =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultIconColorAccent1(context),
                        locationBarBackgroundColorAlphaForNtp);
        mDisableLocationBarRelayout = ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled();
        mUseAdjustedTintColorForNtp =
                NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false);
    }

    @Override
    public void onFinishInflate() {
        try (TraceEvent te = TraceEvent.scoped("ToolbarPhone.onFinishInflate")) {
            super.onFinishInflate();

            mToolbarButtonsContainer = findViewById(R.id.toolbar_buttons);

            if (ChromeFeatureList.sNewTabPageCustomization.isEnabled()
                    && ChromeFeatureList.sNewTabPageCustomizationToolbarButton.isEnabled()) {
                ViewStub homePageButtonsStub = findViewById(R.id.home_page_buttons_stub);

                if (homePageButtonsStub != null) {
                    homePageButtonsStub.inflate();
                }
            }

            mToolbarBackground =
                    new ColorDrawable(getToolbarColorForVisualState(VisualState.NORMAL));
            if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
                setBackground(mToolbarBackground);
            }

            setLayoutTransition(null);

            if (getMenuButtonCoordinator() != null) {
                getMenuButtonCoordinator().setVisibility(true);
            }

            setWillNotDraw(false);
            mUrlFocusTranslationX =
                    getResources().getDimensionPixelSize(R.dimen.toolbar_url_focus_translation_x);

            mActiveLocationBarBackgroundView = findViewById(R.id.location_bar_background_view);
            if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
                mActiveLocationBarBackgroundView.setVisibility(View.VISIBLE);
            }
        }
    }

    @Override
    @Initializer
    public void initialize(
            ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController,
            MenuButtonCoordinator menuButtonCoordinator,
            @Nullable ToggleTabStackButtonCoordinator tabSwitcherButtonCoordinator,
            HistoryDelegate historyDelegate,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Tracker> trackerSupplier,
            ToolbarProgressBar progressBar,
            @Nullable ReloadButtonCoordinator reloadButtonCoordinator,
            @Nullable BackButtonCoordinator backButtonCoordinator,
            @Nullable ForwardButtonCoordinator forwardButtonCoordinator,
            @Nullable HomeButtonDisplay homeButtonDisplay,
            @Nullable ExtensionToolbarCoordinator extensionToolbarCoordinator,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider,
            @Nullable Supplier<Integer> incognitoWindowCountSupplier) {
        assert tabSwitcherButtonCoordinator != null;
        super.initialize(
                toolbarDataProvider,
                tabController,
                menuButtonCoordinator,
                tabSwitcherButtonCoordinator,
                historyDelegate,
                userEducationHelper,
                trackerSupplier,
                progressBar,
                reloadButtonCoordinator,
                backButtonCoordinator,
                forwardButtonCoordinator,
                homeButtonDisplay,
                extensionToolbarCoordinator,
                themeColorProvider,
                incognitoStateProvider,
                /* incognitoWindowCountSupplier= */ null);
        mUserEducationHelper = userEducationHelper;
        mTrackerSupplier = trackerSupplier;
        mHomeButtonDisplay = assumeNonNull(homeButtonDisplay);

        getToolbarDataProvider().addToolbarDataProviderObserver(this);

        mHomeButtonDisplay.updateState(
                mVisualState, mIsHomeButtonEnabled, mIsHomepageNonNtp, urlHasFocus());
    }

    @Override
    @Initializer
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {
        mLocationBar = locationBarCoordinator;
        mLocationBar
                .getAutocompleteRequestTypeSupplier()
                .addObserver((type) -> updateBackgroundHairline(urlHasFocus(), type));
        Resources res = getResources();
        mLocationBarBackgroundVerticalInset =
                res.getDimensionPixelSize(R.dimen.location_bar_vertical_margin);
        mLocationBarBackground = createModernLocationBarBackground(getContext());
        setActiveLocationBarBackground(mLocationBarBackground);
    }

    private void setActiveLocationBarBackground(Drawable background) {
        mActiveLocationBarBackground = background;
        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            mActiveLocationBarBackgroundView.setBackground(mActiveLocationBarBackground);
            updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, mVisualState);
        } else {
            // With the refactor enabled, the drawable belongs to a different view
            // (mActiveLocationBarBackgroundView) that handles its invalidations. Otherwise, we need
            // to manually handle its invalidations since it's not registered as our background
            // drawable.
            mLocationBarBackground.setCallback(this);
        }
    }

    private void updateBackgroundHairline(boolean urlHasFocus, @AutocompleteRequestType int type) {
        if (!urlHasFocus) {
            mLocationBarBackground.setHairlineBehavior(HairlineBehavior.NONE);
            return;
        }

        mLocationBarBackground.setHairlineBehavior(
                type == AutocompleteRequestType.AI_MODE
                                || type == AutocompleteRequestType.IMAGE_GENERATION
                        ? HairlineBehavior.RAINBOW
                        : HairlineBehavior.NONE);
    }

    @Override
    public void invalidateDrawable(Drawable who) {
        if (who == mLocationBarBackground
                && mActiveLocationBarBackground == mLocationBarBackground) {
            invalidate();
        } else {
            super.invalidateDrawable(who);
        }
    }

    @Override
    public void destroy() {
        cancelAnimations();
        Handler handler = getHandler();
        if (handler != null && mDefaultSearchEngineChangedRunnable != null) {
            handler.removeCallbacks(mDefaultSearchEngineChangedRunnable);
        }

        getToolbarDataProvider().removeToolbarDataProviderObserver(this);

        super.destroy();
    }

    /**
     * @param context The activity {@link Context}.
     * @return The drawable for the modern location bar background.
     */
    public static LocationBarBackgroundDrawable createModernLocationBarBackground(Context context) {
        Resources resources = context.getResources();
        var drawable =
                new LocationBarBackgroundDrawable(
                        context,
                        /* cornerRadiusPx= */ resources.getDimensionPixelSize(
                                R.dimen.modern_toolbar_background_corner_radius),
                        /* strokePx= */ resources.getDimensionPixelSize(
                                R.dimen.fusebox_glif_stroke_width),
                        /* blurStrokePx= */ resources.getDimensionPixelSize(
                                R.dimen.fusebox_glif_blur_stroke_width));
        drawable.setBackgroundColor(
                ContextCompat.getColor(context, R.color.toolbar_text_box_bg_color));
        return drawable;
    }

    private static GradientDrawable createNtpFakeboxBackground(Context context) {
        GradientDrawable drawable =
                (GradientDrawable)
                        AppCompatResources.getDrawable(
                                context, R.drawable.home_surface_search_box_background);
        assert drawable != null;
        drawable.mutate();
        return drawable;
    }

    private void updateBackground(final boolean hasFocus) {
        if (hasFocus) {
            mDropdownListScrolled = false;
            setActiveLocationBarBackground(mLocationBarBackground);
        } else if (isLocationBarShownInNtp()) {
            updateToNtpBackground();
        }
    }

    // Replace location bar background with NTB fakebox background.
    private void updateToNtpBackground() {
        if (mNtpFakeboxBackground == null) {
            mNtpFakeboxBackground = createNtpFakeboxBackground(getContext());
        }
        setActiveLocationBarBackground(mNtpFakeboxBackground);
    }

    /** Set the background color of the location bar to appropriately match the theme color. */
    private void updateModernLocationBarColor(@ColorInt int color) {
        if (mCurrentLocationBarColor == color) return;
        mCurrentLocationBarColor = color;
        mLocationBarBackground.setBackgroundColor(color);
        if (mOptionalButtonCoordinator != null) {
            mOptionalButtonCoordinator.setBackgroundColorFilter(color);
        }
    }

    private void updateModernLocationBarCorners() {
        int nonFocusedRadius =
                getResources()
                        .getDimensionPixelSize(R.dimen.modern_toolbar_background_corner_radius);
        int focusedRadius =
                getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_bg_round_corner_radius);
        int radius =
                (int)
                        MathUtils.interpolate(
                                nonFocusedRadius, focusedRadius, mUrlFocusChangeFraction);
        mLocationBarBackground.setCornerRadius(radius);
    }

    /**
     * Get the corresponding location bar color for a toolbar color.
     *
     * @param toolbarColor The color of the toolbar.
     * @return The location bar color.
     */
    private @ColorInt int getLocationBarColorForToolbarColor(@ColorInt int toolbarColor) {
        if (isLocationBarShownInGeneralNtp() || mIsInLoadingPhaseFromNtpToWebpage) {
            return mLocationBarBackgroundColorForNtp;
        }
        return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                getContext(), toolbarColor, isIncognitoBranded(), /* isCustomTab= */ false);
    }

    /**
     * Get the toolbar default color depending on the toolbar's status.
     *
     * @param shouldUseFocusColor True if should return the color for focus state.
     */
    private @ColorInt int getToolbarDefaultColor(boolean shouldUseFocusColor) {
        if (urlHasFocus() || shouldUseFocusColor) {
            if (mDropdownListScrolled) {
                return isIncognitoBranded()
                        ? getContext().getColor(R.color.omnibox_scrolled_bg_incognito)
                        : ContextCompat.getColor(getContext(), R.color.toolbar_text_box_bg_color);
            }
            return OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                    getContext(), mThemeColorProvider.getBrandedColorScheme());
        }
        return ChromeColors.getDefaultThemeColor(getContext(), isIncognitoBranded());
    }

    /**
     * Get the corresponding default location bar color for a toolbar color. If location bar has
     * focus, return the Omnibox suggestion background color. If location bar does not have focus,
     * return {@link getLocationBarColorForToolbarColor(int toolbarColor)}.
     *
     * @param toolbarColor The color of the toolbar.
     * @param shouldUseFocusColor True if should return the color for focus state.
     * @return The default location bar color.
     */
    private @ColorInt int getLocationBarDefaultColorForToolbarColor(
            @ColorInt int toolbarColor, boolean shouldUseFocusColor) {
        if (urlHasFocus() || shouldUseFocusColor) {

            // Omnibox has same background as the Omnibox suggestion.
            return OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                    getContext(), mThemeColorProvider.getBrandedColorScheme());
        }
        return getLocationBarColorForToolbarColor(toolbarColor);
    }

    /** Sets up click and key listeners once we have native library available to handle clicks. */
    @Override
    protected void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        updateVisualsForLocationBarState();
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // If the NTP is partially scrolled, prevent all touch events to the child views.  This
        // is to not allow a secondary touch event to trigger entering the tab switcher, which
        // can lead to really odd snapshots and transitions to the switcher.
        if (mNtpSearchBoxScrollFraction != 0f
                && mNtpSearchBoxScrollFraction != 1f
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
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // In case the call came from the handler after we destroyed the dependencies, skip the work
        // that could touch the already destroyed objects.
        if (mDestroyChecker.isDestroyed()) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }
        if (!mDisableLocationBarRelayout) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);

            boolean changed =
                    layoutLocationBarWithoutAnimationExpansion(
                            MeasureSpec.getSize(widthMeasureSpec));
            // Avoid URL expansion while in or transitioning to/from tab switcher.
            // updateUrlExpansionAnimation() is called for these states via
            // setTabSwitcherMode()/onTabSwitcherTransitionFinished() ->
            // updateVisualsForLocationBarState()
            if (!isInTabSwitcherMode()) {
                invokeTransition();
            }
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

    void setLocationBarBackgroundDrawableForTesting(LocationBarBackgroundDrawable background) {
        mLocationBarBackground = background;
    }

    void setOptionalButtonCoordinatorForTesting(
            OptionalButtonCoordinator optionalButtonCoordinator) {
        mOptionalButtonCoordinator = optionalButtonCoordinator;
    }

    @SuppressLint("RtlHardcoded")
    private boolean layoutLocationBarWithoutAnimationExpansion(int containerWidth) {
        // Note that Toolbar's direction depends on system layout direction while
        // LocationBar's direction depends on its text inside.
        MarginLayoutParams locationBarLayoutParams =
                mLocationBar.getPhoneCoordinator().getMarginLayoutParams();

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
            width =
                    getFocusedLocationBarWidth(
                            containerWidth, priorVisibleWidth, mLayoutLocationBarInFocusedMode);
            leftMargin =
                    getFocusedLocationBarLeftMargin(
                            priorVisibleWidth, mLayoutLocationBarInFocusedMode);
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
     * @param isInFocusedMode Whether the current location bar is in focused mode.
     * @return The width of the location bar when it has focus.
     */
    private int getFocusedLocationBarWidth(
            int containerWidth, int priorVisibleWidth, boolean isInFocusedMode) {
        int width =
                containerWidth
                        - (2 * (isInFocusedMode ? mToolbarSidePadding : mToolbarSidePaddingForNtp))
                        + priorVisibleWidth;

        return width;
    }

    /**
     * @param priorVisibleWidth The width of any visible views prior to the location bar.
     * @param isInFocusedMode Whether the current location bar is in focused mode.
     * @return The left margin of the location bar when it has focus.
     */
    private int getFocusedLocationBarLeftMargin(int priorVisibleWidth, boolean isInFocusedMode) {
        int baseMargin = (isInFocusedMode ? mToolbarSidePadding : mToolbarSidePaddingForNtp);
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
            return mToolbarSidePaddingForNtp;
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
        int padding = mToolbarSidePaddingForNtp;

        // If home button is visible, homeButton.getMeasuredWidth() should be returned as the left
        // bound.
        if (mHomeButtonDisplay.getVisibility() != GONE) {
            padding = mHomeButtonDisplay.getMeasuredWidth();
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
            return getMeasuredWidth() - mToolbarSidePaddingForNtp;
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
        int toolbarButtonsContainerWidth = mToolbarButtonsContainer.getMeasuredWidth();

        // MeasuredWidth() represents the desired width of the container which is accurate most
        // time, except during the optional button animations, where the MeasuredWidth changes
        // instantly to the final size and Width() represents the actual size at that frame.
        if (mOptionalButtonAnimationRunning) {
            toolbarButtonsContainerWidth = mToolbarButtonsContainer.getWidth();
        }

        return Math.max(mToolbarSidePaddingForNtp, toolbarButtonsContainerWidth);
    }

    private void updateToolbarBackground(@ColorInt int color) {
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

    private @ColorInt int getToolbarColorForVisualState(final @VisualState int visualState) {
        switch (visualState) {
            case VisualState.NEW_TAB_NORMAL:
                // We are likely in the middle of a layout animation, and the NTP cannot draw itself
                // yet. Use the NTP background color, which will match what the NTP eventually
                // draws itself.
                if (!getToolbarDataProvider().getNewTabPageDelegate().hasCompletedFirstLayout()) {
                    return mToolbarBackgroundColorForNtp;
                }

                // During transition we cannot rely on the background to be opaque yet, so keep full
                // alpha until the transition is over.
                int alpha = mInLayoutTransition ? 255 : Math.round(mUrlExpansionFraction * 255);

                // When the NTP fake search box is visible, the background color should be
                // transparent. When the location bar reaches the top of the screen (i.e. location
                // bar is fully expanded), the background needs to change back to the NTP toolbar
                // color so that the NTP content is not visible beneath the toolbar. In between the
                // transition, we set a translucent NTP toolbar color based on the expansion
                // progress of the toolbar.
                return ColorUtils.setAlphaComponent(mToolbarBackgroundColorForNtp, alpha);
            case VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO:
                return mToolbarBackgroundColorForNtp;
            case VisualState.NORMAL:
                if (mIsInLoadingPhaseFromNtpToWebpage) {
                    return mToolbarBackgroundColorForNtp;
                }
                return ChromeColors.getDefaultThemeColor(getContext(), /* isIncognito= */ false);
            case VisualState.INCOGNITO:
                return ChromeColors.getDefaultThemeColor(getContext(), /* isIncognito= */ true);
            case VisualState.BRAND_COLOR:
                if (urlHasFocus()) {
                    return getToolbarDefaultColor(/* shouldUseFocusColor= */ false);
                }
                return getToolbarDataProvider().getPrimaryColor();
            default:
                assert false;
                return ChromeColors.getDefaultThemeColor(getContext(), /* isIncognito= */ false);
        }
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (!mTextureCaptureMode && mToolbarBackground.getColor() != Color.TRANSPARENT) {
            // Update to compensate for orientation changes.
            mToolbarBackground.setBounds(0, 0, getWidth(), getHeight());
            if (!ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
                mToolbarBackground.draw(canvas);
            } else {
                mToolbarBackground.setAlpha(255);
            }
        } else if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            // TODO(crbug.com/425817689): Move the flag-guarded setAlpha calls outside of the draw
            //  cycle. Specifically, move to where we set the state that determines whether or not
            //  the background should be drawn (capture mode && transparent color).
            mToolbarBackground.setAlpha(0);
        }

        if (mLocationBarBackground != null
                && (mLocationBar.getPhoneCoordinator().getVisibility() == VISIBLE
                        || mTextureCaptureMode)
                && !ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, mVisualState);
        }

        if (mTextureCaptureMode) {
            drawWithoutBackground(canvas);
        } else {
            super.dispatchDraw(canvas);
        }
    }

    @Override
    protected boolean verifyDrawable(Drawable who) {
        return super.verifyDrawable(who) || who == mActiveLocationBarBackground;
    }

    private void onNtpScrollChanged(float scrollFraction) {
        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
            if (!ntpDelegate.hasCompletedFirstLayout()) {
                // If the NTP has not completed its first layout, but we're getting a scroll update,
                // we're likely navigating to a NTP. Immediately hide the location bar in this case,
                // as the fakebox will soon show.
                if (isLocationBarShownInNtp()) {
                    mLocationBar.getPhoneCoordinator().setAlpha(0.f);
                    mActiveLocationBarBackgroundView.setAlpha(0.f);
                }
                // Also skip the updates below, since we won't be able to derive the correct
                // properties (translationY, etc.) yet.
                return;
            }

            updateButtonsTranslationY();
            updateToolbarBackgroundFromState(mVisualState);
            updateLocationBarTranslationOnScroll();
            float oldScrollFraction = mNtpSearchBoxScrollFraction;
            if (scrollFraction > 0.f && mNtpSearchBoxScrollFraction != 1.f) {
                mNtpSearchBoxScrollFraction = 1.f;
                createAndRunNtpFocusAnimatorRefactored();
            } else if (scrollFraction <= 0.f && mNtpSearchBoxScrollFraction != 0.f) {
                mNtpSearchBoxScrollFraction = 0.f;
                createAndRunNtpFocusAnimatorRefactored();
            }
            // If the scroll fraction was previously uninitialized, we likely just navigated to a
            // NTP. We want to update the location bar's state, but suppress the associated
            // animations, so allow the transition to start, then immediately end it here.
            if (oldScrollFraction == UNINITIALIZED_FRACTION) {
                endFocusTransition(/* hasFocus= */ false);
            }
            return;
        }

        mNtpSearchBoxScrollFraction = scrollFraction;
        if (mNtpSearchBoxScrollFraction > 0) {
            updateLocationBarForNtp(mVisualState, urlHasFocus());
        }

        if (mUseAdjustedTintColorForNtp) {
            if (!mIsToolbarExpandedOnNtp && mNtpSearchBoxScrollFraction > 0) {
                mIsToolbarExpandedOnNtp = true;
                notifyToolbarExpandingOnNtp(true);
            } else if (mIsToolbarExpandedOnNtp && mNtpSearchBoxScrollFraction == 0) {
                mIsToolbarExpandedOnNtp = false;
                notifyToolbarExpandingOnNtp(false);
            }
        }
        updateUrlExpansionFraction();
        invokeTransition();
    }

    private void updateLocationBarTranslationOnScroll() {
        // When the scroll fraction is set to 1.f, the location bar is "snapped" to the top of the
        // screen, so we don't update the y-translation here.
        if (mNtpSearchBoxScrollFraction != 1.f) {
            // We intentionally do not call #onLocationBarBackgroundViewBoundsChanged here, since
            // the size of the background will instead be handled by the refactored transition.
            float translationY = getLocationBarTranslationY() - mRefactoredNtpStartingOffset;
            mLocationBar.getPhoneCoordinator().setTranslationY(translationY);
            mActiveLocationBarBackgroundView.setTranslationY(translationY);
        }
    }

    private float getLocationBarTranslationY() {
        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        ntpDelegate.getSearchBoxBounds(mNtpSearchBoxBounds, mNtpSearchBoxTranslation);
        float translationY = mNtpSearchBoxBounds.top - mLocationBar.getPhoneCoordinator().getTop();
        return Math.max(0, translationY);
    }

    private void createAndRunNtpFocusAnimatorRefactored() {
        mRefactoredNtpStartingOffset = getLocationBarTranslationY();
        // TODO(crbug.com/462492387): Cleanup this flow. It's unclear if we still need to call
        //  #updateLocationBarForNtp here. We likely can also refactor the transition helpers, so
        //  we don't #triggerUrlFocusAnimation here, and instead only set up the transitions
        //  needed for this flow (size change, translation, etc.).
        updateLocationBarForNtp(mVisualState, /* hasFocus= */ false);
        triggerUrlFocusAnimation(/* hasFocus= */ false);
    }

    /**
     * @return True if the toolbar is showing tab switcher assets, including during transitions.
     */
    public boolean isInTabSwitcherMode() {
        return mTabSwitcherState != STATIC_TAB;
    }

    /** Calculate the bounds for the location bar background and set them to {@code out}. */
    private void updateLocationBarBackgroundBounds(Rect out, @VisualState int visualState) {
        // Calculate the visible boundaries of the left and right most child views of the
        // location bar.
        int leftViewPosition = getLeftPositionOfLocationBarBackground(visualState);
        int rightViewPosition = getRightPositionOfLocationBarBackground(visualState);
        int verticalInset = mLocationBarBackgroundVerticalInset - calculateOnFocusHeightIncrease();
        int horizontalInset = 0;

        if (urlHasFocus() && ChromeFeatureList.sAndroidBottomToolbarV2.isEnabled()) {
            int strokePx =
                    getContext().getResources().getDimensionPixelSize(R.dimen.chip_border_width);
            verticalInset -= strokePx;
            horizontalInset = strokePx;
        }

        // The bounds are set by the following:
        // - The left most visible location bar child view.
        // - The top of the viewport is aligned with the top of the location bar.
        // - The right most visible location bar child view.
        // - The bottom of the viewport is aligned with the bottom of the location bar.
        // Additional padding can be applied for use during animations.
        out.set(
                leftViewPosition - horizontalInset,
                mLocationBar.getPhoneCoordinator().getTop() + verticalInset,
                rightViewPosition + horizontalInset,
                mLocationBar.getPhoneCoordinator().getBottom() - verticalInset);
        onLocationBarBackgroundViewBoundsChanged();
    }

    /**
     * @param visualState The current {@link VisualState} of the toolbar.
     * @return The left drawing position for the location bar background.
     */
    private int getLeftPositionOfLocationBarBackground(@VisualState int visualState) {
        float expansion = getExpansionFractionForVisualState(visualState);
        int leftViewPosition =
                (int)
                        MathUtils.interpolate(
                                getViewBoundsLeftOfLocationBar(visualState),
                                getFocusedLeftPositionOfLocationBarBackground(),
                                expansion);

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
                (int)
                        MathUtils.interpolate(
                                getViewBoundsRightOfLocationBar(visualState),
                                getFocusedRightPositionOfLocationBarBackground(),
                                expansion);

        return rightViewPosition;
    }

    /**
     * @return The difference in the location bar width when the optional button is hidden rather
     *     than showing. This is effectively the width of the optional button with some adjustment
     *     to account for possible padding differences when the button visibility changes.
     */
    @VisibleForTesting
    @NullUnmarked
    float getLocationBarWidthOffsetForOptionalButton() {
        float widthChange = mOptionalButtonCoordinator.getViewWidth();

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
                ? mUrlFocusChangeFraction
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
        invokeTransition();
    }

    private void updateUrlExpansionFraction() {
        mUrlExpansionFraction = Math.max(mNtpSearchBoxScrollFraction, mUrlFocusChangeFraction);
        assert mUrlExpansionFraction >= 0;
        assert mUrlExpansionFraction <= 1;
    }

    /**
     * Updates the parameters relating to expanding the location bar, as the result of either a
     * focus change or scrolling the New Tab Page.
     */
    private void updateUrlExpansionAnimation() {
        // TODO(crbug.com/40585866): Prevent url expansion signals from happening while the
        // toolbar is not visible (e.g. in tab switcher mode).
        if (isInTabSwitcherMode()) return;

        int toolbarButtonVisibility = getToolbarButtonVisibility();
        mToolbarButtonsContainer.setVisibility(toolbarButtonVisibility);
        if (mHomeButtonDisplay.getVisibility() != GONE) {
            mHomeButtonDisplay.setVisibility(toolbarButtonVisibility);
        }

        updateLocationBarLayoutForExpansionAnimation();
    }

    /**
     * @return The visibility for {@link #mToolbarButtonsContainer}.
     */
    private int getToolbarButtonVisibility() {
        return (mUrlExpansionFraction == 1f) ? INVISIBLE : VISIBLE;
    }

    /**
     * Updates the location bar layout, as the result of either a focus change or scrolling the New
     * Tab Page.
     */
    private void updateLocationBarLayoutForExpansionAnimation() {
        TraceEvent.begin("ToolbarPhone.updateLocationBarLayoutForExpansionAnimation");
        if (isInTabSwitcherMode()) return;

        MarginLayoutParams locationBarLayoutParams =
                mLocationBar.getPhoneCoordinator().getMarginLayoutParams();
        int currentLeftMargin = locationBarLayoutParams.leftMargin;
        int currentWidth = locationBarLayoutParams.width;

        boolean isInNtp = mVisualState == VisualState.NEW_TAB_NORMAL;
        float locationBarBaseTranslationX =
                (mUrlFocusChangeInProgress && isInNtp
                                ? getFocusedLeftPositionOfLocationBarBackground()
                                : mUnfocusedLocationBarLayoutLeft)
                        - currentLeftMargin;
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
        var profile = getToolbarDataProvider().getProfile();
        if (profile == null
                || SearchEngineUtils.getForProfile(profile).shouldShowSearchEngineLogo()) {
            locationBarBaseTranslationX += getLocationBarOffsetForFocusAnimation(hasFocus());
        }

        boolean isLocationBarRtl =
                mLocationBar.getPhoneCoordinator().getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if (isLocationBarRtl) {
            locationBarBaseTranslationX += mUnfocusedLocationBarLayoutWidth - currentWidth;
        }

        locationBarBaseTranslationX *= 1f - mUrlExpansionFraction;

        boolean isLocationBarShownInNtp = isLocationBarShownInNtp();

        float locationBarTranslationX;
        if (isLocationBarRtl) {
            locationBarTranslationX = locationBarBaseTranslationX + mLocationBarNtpOffsetRight;
        } else {
            locationBarTranslationX = locationBarBaseTranslationX + mLocationBarNtpOffsetLeft;
        }

        mLocationBar.getPhoneCoordinator().setTranslationX(locationBarTranslationX);

        if (!mOptionalButtonAnimationRunning) {
            boolean isUrlFocusChangeInProgressWithScrollCompleted =
                    mNtpSearchBoxScrollFraction == 1 && mUrlFocusChangeInProgress;

            mLocationBar.setLocationBarButtonTranslationForNtpAnimation(
                    getUrlActionsTranslationXForExpansionAnimation(
                            isLocationBarRtl,
                            locationBarBaseTranslationX,
                            isUrlFocusChangeInProgressWithScrollCompleted));

            // A url expansion fraction < 1.0 fades and translates the DSE icon away from its final
            // state. If the DSE icon is always visible on the NTP, it should stay at full alpha and
            // in its final location rather than being affected by scroll offset.
            float ntpUrlExpansionFraction =
                    isLocationBarShownInNtp ? 1.0f : mNtpSearchBoxScrollFraction;
            mLocationBar.setUrlFocusChangeFraction(
                    ntpUrlExpansionFraction, mUrlFocusChangeFraction);

            // Update the location bar background color and corner radius using fraction.
            updateToolbarAndLocationBarColorForFocusChange();
        }

        // Force an invalidation of the location bar to properly handle the clipping of the URL
        // bar text as a result of the URL action container translations.
        mLocationBar.getPhoneCoordinator().invalidate();
        invalidate();
        TraceEvent.end("ToolbarPhone.updateLocationBarLayoutForExpansionAnimation");
    }

    private void updateToolbarAndLocationBarColorForFocusChange() {
        if (mOptionalButtonAnimationRunning) return;
        // Only transition theme colors if in static tab mode that is not the NTP or while
        // focusing on the NTP. In NTP, toolbar and locationbar need to transite color only when
        // the omnibox is focused. When the fake omnibox is scrolled, the color should not
        // change.
        if ((urlHasFocus() || !isLocationBarShownInNtp()) && mTabSwitcherState == STATIC_TAB) {
            boolean isInGeneralNtp =
                    isLocationBarShownInGeneralNtp() || mIsInLoadingPhaseFromNtpToWebpage;
            // Add a special case for general NTP to the defaultColor to ensure that the color
            // is right and changes smoothly during the un-focus animation.
            boolean shouldUseFocusColor = isInGeneralNtp && mUrlFocusChangeInProgress;
            @ColorInt int defaultColor = getToolbarDefaultColor(shouldUseFocusColor);
            @ColorInt
            int defaultLocationBarColor =
                    getLocationBarDefaultColorForToolbarColor(defaultColor, shouldUseFocusColor);
            @ColorInt
            int primaryColor =
                    isInGeneralNtp
                            ? mToolbarBackgroundColorForNtp
                            : getToolbarDataProvider().getPrimaryColor();
            @ColorInt int themedLocationBarColor = getLocationBarColorForToolbarColor(primaryColor);

            updateToolbarBackground(
                    ColorUtils.blendColorsMultiply(
                            primaryColor, defaultColor, mUrlFocusChangeFraction));
            updateModernLocationBarColor(
                    ColorUtils.blendColorsMultiply(
                            themedLocationBarColor,
                            defaultLocationBarColor,
                            mUrlFocusChangeFraction));

            updateModernLocationBarCorners();
        }
    }

    /**
     * Calculates the translation X for the URL actions container for use in the URL expansion
     * animation.
     *
     * @param isLocationBarRtl Whether the location bar layout is RTL.
     * @param locationBarBaseTranslationX The base location bar translation for the URL expansion
     *     animation.
     * @param isUrlFocusChangeInProgressWithScrollCompleted True if it is a focus or un-focus
     *     animation while the real omnibox is visible on NTP.
     * @return The translation X for the URL actions container.
     */
    private float getUrlActionsTranslationXForExpansionAnimation(
            boolean isLocationBarRtl,
            float locationBarBaseTranslationX,
            boolean isUrlFocusChangeInProgressWithScrollCompleted) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        float urlActionsTranslationX = 0;

        if (isUrlFocusChangeInProgressWithScrollCompleted) {
            int urlActionContainerEndMarginChange =
                    getResources().getDimensionPixelSize(R.dimen.location_bar_url_action_offset_ntp)
                            - getResources()
                                    .getDimensionPixelSize(R.dimen.location_bar_url_action_offset);
            int toolbarSidePaddingChange = mToolbarSidePaddingForNtp - mToolbarSidePadding;
            if (urlHasFocus()) {
                urlActionsTranslationX =
                        MathUtils.flipSignIf(
                                (toolbarSidePaddingChange + urlActionContainerEndMarginChange)
                                        * (mUrlFocusChangeFraction - 1),
                                isRtl);
            } else {
                urlActionsTranslationX =
                        MathUtils.flipSignIf(
                                toolbarSidePaddingChange * (mUrlFocusChangeFraction - 1)
                                        + urlActionContainerEndMarginChange
                                                * mUrlFocusChangeFraction,
                                isRtl);
            }
            return urlActionsTranslationX;
        }

        if (!isLocationBarRtl || isRtl) {
            // Negate the location bar translation to keep the URL action container in the same
            // place during the focus expansion.
            urlActionsTranslationX = -locationBarBaseTranslationX;
        }

        if (isRtl) {
            urlActionsTranslationX +=
                    mLocationBarNtpOffsetLeft
                            - mLocationBarNtpOffsetRight
                            + ((mVisualState == VisualState.NEW_TAB_NORMAL)
                                    ? mUrlActionsNtpEndOffset
                                    : 0);
        } else {
            urlActionsTranslationX +=
                    mLocationBarNtpOffsetRight
                            - mLocationBarNtpOffsetLeft
                            - ((mVisualState == VisualState.NEW_TAB_NORMAL)
                                    ? mUrlActionsNtpEndOffset
                                    : 0);
        }

        return urlActionsTranslationX;
    }

    /**
     * Reset the parameters for the New Tab Page transition animation (expanding the location bar as
     * a result of scrolling the New Tab Page) to their default values.
     */
    private void resetNtpAnimationValues() {
        mLocationBarBackgroundNtpOffset.setEmpty();
        onLocationBarBackgroundViewBoundsChanged();
        mLocationBarNtpOffsetLeft = 0;
        mLocationBarNtpOffsetRight = 0;
        mRefactoredNtpStartingOffset = 0;
        setActiveLocationBarBackground(mLocationBarBackground);
        mNtpSearchBoxTranslation.set(0, 0);
        mLocationBar.getPhoneCoordinator().setTranslationY(0);
        mLocationBar.getPhoneCoordinator().setTranslationX(0);
        if (!mUrlFocusChangeInProgress) {
            mToolbarButtonsContainer.setTranslationY(0);
            mHomeButtonDisplay.setTranslationY(0);
        }

        if (!mUrlFocusChangeInProgress && getToolbarShadow() != null) {
            getToolbarShadow().setAlpha(urlHasFocus() ? 0.f : 1.f);
        }

        mLocationBar.getPhoneCoordinator().setAlpha(1);
        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            mActiveLocationBarBackgroundView.setAlpha(1.f);
        }
        mForceDrawLocationBarBackground = false;

        setAncestorsShouldClipChildren(true);
        setClipToPadding(true);
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
        // If this feature is enabled, this is handled by refactored transitions instead.
        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            // TODO(crbug.com/425817689): We still need to update the color here, though, since this
            //  needs to be called after the expansion fraction has been updated, but before
            //  #updateToolbarAndLocationBarColorForFocusChange, which may override this color.
            updateToolbarBackgroundFromState(mVisualState);
            return;
        }

        boolean isExpanded = mUrlExpansionFraction > 0f;
        boolean isPartiallyExpanded = isExpanded && mUrlExpansionFraction < 1f;
        // We only need to avoid getting clipped in the period where the fakebox drawable belongs to
        // us but is taller than our bounds. It doesn't yet belong to us when the expansion fraction
        // is 0 and by design matches our height when the expansion fraction is 1. Restoring
        // clipChildren as soon as possible helps prevent visual glitches caused by views
        // inadvertently drawing out of their bounds and on top of each other, e.g.
        // crbug.com/396456418
        setAncestorsShouldClipChildren(!isPartiallyExpanded);
        setClipToPadding(!isExpanded);
        if (!mUrlFocusChangeInProgress) {
            float alpha = 0.f;
            if (!urlHasFocus() && mNtpSearchBoxScrollFraction == 1.f) {
                alpha = 1.f;
            }
            getToolbarShadow().setAlpha(alpha);
        }

        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        ntpDelegate.setUrlFocusChangeAnimationPercent(mUrlFocusChangeFraction);
        // #getSearchBoxBounds is only valid once the NTP can actually draw itself.
        if (ntpDelegate.hasCompletedFirstLayout()) {
            ntpDelegate.getSearchBoxBounds(mNtpSearchBoxBounds, mNtpSearchBoxTranslation);
            int locationBarTranslationY = (int) getLocationBarTranslationY();
            mLocationBar.getPhoneCoordinator().setTranslationY(locationBarTranslationY);

            updateButtonsTranslationY();

            // Linearly interpolate between the bounds of the search box on the NTP and the omnibox
            // background bounds. |shrinkage| is the scaling factor for the offset -- if it's 1, we
            // are shrinking the omnibox down to the size of the search box.
            float shrinkage =
                    1f
                            - Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR.getInterpolation(
                                    mUrlExpansionFraction);

            int leftBoundDifference =
                    mUrlFocusChangeInProgress
                            ? 0
                            : mNtpSearchBoxBounds.left - mLocationBarBackgroundBounds.left;
            int rightBoundDifference =
                    mUrlFocusChangeInProgress
                            ? 0
                            : mNtpSearchBoxBounds.right - mLocationBarBackgroundBounds.right;
            mLocationBarBackgroundNtpOffset.set(
                    Math.round(leftBoundDifference * shrinkage),
                    locationBarTranslationY,
                    Math.round(rightBoundDifference * shrinkage),
                    locationBarTranslationY);
            float urlExpansionFractionComplement = 1.f - mUrlExpansionFraction;
            var resources = getResources();
            int verticalInset =
                    (int) (getNtpTransitionVerticalInset() * urlExpansionFractionComplement);

            int locationBarUrlActionOffsetChange =
                    resources.getDimensionPixelSize(R.dimen.location_bar_url_action_offset_ntp)
                            - resources.getDimensionPixelSize(
                                    R.dimen.location_bar_url_action_offset);
            int focusChangeDelta = mUrlFocusChangeInProgress ? 0 : locationBarUrlActionOffsetChange;
            mUrlActionsNtpEndOffset =
                    (resources.getDimensionPixelSize(R.dimen.fake_search_box_end_padding)
                                    - focusChangeDelta)
                            * shrinkage;
            mLocationBarBackgroundNtpOffset.inset(0, verticalInset);

            if (mUrlFocusChangeInProgress) {
                leftBoundDifference =
                        mNtpSearchBoxBounds.left - getFocusedLeftPositionOfLocationBarBackground();
                rightBoundDifference =
                        mNtpSearchBoxBounds.right
                                - getFocusedRightPositionOfLocationBarBackground();
            }
            mLocationBarNtpOffsetLeft = leftBoundDifference * shrinkage;
            mLocationBarNtpOffsetRight = rightBoundDifference * shrinkage;
        }

        mForceDrawLocationBarBackground = isExpanded;
        float relativeAlpha = isExpanded ? 1f : 0f;
        mLocationBar.getPhoneCoordinator().setAlpha(relativeAlpha);

        // The search box on the NTP is visible if our omnibox is invisible, and vice-versa.
        ntpDelegate.setSearchBoxAlpha(1f - relativeAlpha);

        updateToolbarBackgroundFromState(mVisualState);
    }

    private float getNtpTransitionVerticalInset() {
        var resources = getResources();
        int baseInset =
                resources.getDimensionPixelSize(R.dimen.modern_toolbar_background_size)
                        - resources.getDimensionPixelSize(R.dimen.ntp_search_box_height);
        return baseInset / 2.f;
    }

    /**
     * Update the y translation of the buttons to make it appear as if they were scrolling with the
     * new tab page.
     */
    private void updateButtonsTranslationY() {
        int transY = mTabSwitcherState == STATIC_TAB ? Math.min(mNtpSearchBoxTranslation.y, 0) : 0;

        mToolbarButtonsContainer.setTranslationY(transY);
        mHomeButtonDisplay.setTranslationY(transY);
    }

    private void setAncestorsShouldClipChildren(boolean clip) {
        if (!isLocationBarShownInNtp()) return;

        ViewUtils.setAncestorsShouldClipChildren(this, clip);
    }

    /** Draws all the browsing mode views at full alpha, but without a background. */
    @VisibleForTesting
    void drawWithoutBackground(Canvas canvas) {
        if (!isNativeLibraryReady()) return;

        int rgbAlpha = 255;
        canvas.save();
        canvas.clipRect(mBackgroundOverlayBounds);

        if (mHomeButtonDisplay.getVisibility() != GONE) {
            drawChild(canvas, mHomeButtonDisplay.getView(), SystemClock.uptimeMillis());
        }

        // Draw the location/URL bar.
        if (mLocationBar.getPhoneCoordinator().getAlpha() != 0 && isLocationBarCurrentlyShown()) {
            drawLocationBar(canvas, SystemClock.uptimeMillis());
        }

        // Translate to draw end toolbar buttons.
        ViewUtils.translateCanvasToView(this, mToolbarButtonsContainer, canvas);

        // Draw the optional button if visible. We check for both visibility and width because in
        // some cases (e.g. the first frame of the showing animation) the view may be visible with a
        // width of zero. Calling draw in this state results in drawing the inner ImageButton when
        // it's not supposed to. (See https://crbug.com/1422176 for an example of this happening).
        if (mOptionalButtonCoordinator != null
                && mOptionalButtonCoordinator.getViewVisibility() != View.GONE
                && mOptionalButtonCoordinator.getViewWidth() != 0) {
            canvas.save();
            ViewUtils.translateCanvasToView(
                    mToolbarButtonsContainer,
                    mOptionalButtonCoordinator.getViewForDrawing(),
                    canvas);
            mOptionalButtonCoordinator.getViewForDrawing().draw(canvas);
            canvas.restore();
        }

        // Draw the tab stack button and associated text if necessary.
        if (getTabSwitcherButtonCoordinator() != null && mUrlExpansionFraction != 1f) {
            // Draw the tab stack button image.
            getTabSwitcherButtonCoordinator().draw(mToolbarButtonsContainer, canvas);
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
        // For other views, fall back to the default implementation when the location bar relayout
        // is disabled.
        if (mDisableLocationBarRelayout
                && ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            return super.drawChild(canvas, child, drawingTime);
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
                    int clipRight =
                            verticalClip
                                    ? child.getMeasuredWidth()
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
        return (child == mHomeButtonDisplay.getView()) ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * @return Whether or not the location bar should be drawing at any particular state of the
     *         toolbar.
     */
    private boolean shouldDrawLocationBar() {
        // The location bar should have alpha or clip+translation when its not supposed to be
        // shown. Needs to be drawn during transitions, such as entering/exiting tab switcher.
        return mLocationBarBackground != null;
    }

    /** Called whenever the location bar background view's bounds or its NTP offset changes. */
    private void onLocationBarBackgroundViewBoundsChanged() {
        if (!ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            return;
        }
        updateLocationBarBackgroundViewBounds();
    }

    /**
     * Updates the toolbar animation refactor's location bar background host view's position, based
     * on its previously calculated bounds, the NTP offset, and the location bar's translation.
     */
    private void updateLocationBarBackgroundViewBounds() {
        int left = mLocationBarBackgroundBounds.left + mLocationBarBackgroundNtpOffset.left;
        int top = mLocationBarBackgroundBounds.top + mLocationBarBackgroundNtpOffset.top;
        int right = mLocationBarBackgroundBounds.right + mLocationBarBackgroundNtpOffset.right;
        int bottom = mLocationBarBackgroundBounds.bottom + mLocationBarBackgroundNtpOffset.bottom;

        mActiveLocationBarBackgroundView.setTranslationX(left);
        mActiveLocationBarBackgroundView.setTranslationY(top);
        ViewGroup.LayoutParams lp = mActiveLocationBarBackgroundView.getLayoutParams();
        lp.width = right - left;
        lp.height = bottom - top;
        mActiveLocationBarBackgroundView.setLayoutParams(lp);
    }

    private boolean drawLocationBar(Canvas canvas, long drawingTime) {
        TraceEvent.begin("ToolbarPhone.drawLocationBar");
        boolean clipped = false;
        if (shouldDrawLocationBar()) {
            canvas.save();
            // If the animation refactor is enabled, the background will instead be drawn by an
            // Android view hosting the background drawable.
            if (shouldDrawLocationBarBackground()
                    && !ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
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
                int leftDelta =
                        mUnfocusedLocationBarLayoutLeft
                                - getViewBoundsLeftOfLocationBar(mVisualState);
                int rightDelta =
                        getViewBoundsRightOfLocationBar(mVisualState)
                                - mUnfocusedLocationBarLayoutLeft
                                - mUnfocusedLocationBarLayoutWidth;
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
            var profile = getToolbarDataProvider().getProfile();
            if ((profile == null
                            || SearchEngineUtils.getForProfile(profile)
                                    .shouldShowSearchEngineLogo())
                    && isLocationBarShownInNtp()
                    && urlHasFocus()
                    && mUrlFocusChangeInProgress) {
                if (locationBarDirection == LAYOUT_DIRECTION_RTL) {
                    locationBarClipRight -= locationBarPaddingStart;
                } else {
                    locationBarClipLeft += locationBarPaddingStart;
                }
            }

            // Clip the location bar child to the URL viewport calculated in onDraw.
            canvas.clipRect(
                    locationBarClipLeft,
                    locationBarClipTop,
                    locationBarClipRight,
                    locationBarClipBottom);
            clipped = true;
        }

        // TODO(crbug.com/40151029): Hide this View interaction if possible.
        boolean retVal =
                super.drawChild(
                        canvas,
                        mLocationBar.getPhoneCoordinator().getViewForDrawing(),
                        drawingTime);

        if (clipped) canvas.restore();
        TraceEvent.end("ToolbarPhone.drawLocationBar");
        return retVal;
    }

    /**
     * @return Whether the location bar background should be drawn in {@link
     *     #drawLocationBar(Canvas, long)}.
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
    public void draw(Canvas canvas) {
        if (mDestroyChecker.isDestroyed()) return;
        // If capturing a texture of the toolbar, ensure the alpha is set prior to draw(...) being
        // called.  The alpha is being used prior to getting to draw(...), so updating the value
        // after this point was having no affect.
        assert !mTextureCaptureMode || getAlpha() == 1f;
        super.draw(canvas);
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        return getReadinessStateWithSuppression();
    }

    private CaptureReadinessResult getReadinessStateWithSuppression() {
        if (isLayoutRequested() || isInLayout()) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.LAYOUT_REQUESTED);
        } else if (urlHasFocus()) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS);
        } else if (mUrlFocusChangeInProgress) {
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.URL_BAR_FOCUS_IN_PROGRESS);
        } else if (mOptionalButtonAnimationRunning) {
            long timeSinceAnimationStartMs =
                    TimeUtils.elapsedRealtimeMillis() - mOptionalButtonAnimationStartTimeMs;
            if (timeSinceAnimationStartMs
                    > CaptureReadinessResult.STUCK_ON_ANIMATION_BLOCK_REASON_THRESHOLD_MS) {
                RecordHistogram.recordMediumTimesHistogram(
                        "Android.TopToolbar.CaptureBlocked.OptionalButtonAnimationDuration",
                        timeSinceAnimationStartMs);
            }
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS);
        } else if (mLocationBar.getStatusCoordinator() != null
                && mLocationBar.getStatusCoordinator().isStatusIconAnimating()) {
            long timeSinceAnimationStartMs =
                    TimeUtils.elapsedRealtimeMillis()
                            - mLocationBar.getStatusCoordinator().getAnimationStartTimeMs();
            if (timeSinceAnimationStartMs
                    > CaptureReadinessResult.STUCK_ON_ANIMATION_BLOCK_REASON_THRESHOLD_MS) {
                RecordHistogram.recordMediumTimesHistogram(
                        "Android.TopToolbar.CaptureBlocked.StatusIconAnimationDuration",
                        timeSinceAnimationStartMs);
            }
            // TODO(crbug.com/40860241): It may be possible to remove the above null check.
            return CaptureReadinessResult.notReady(
                    TopToolbarBlockCaptureReason.STATUS_ICON_ANIMATION_IN_PROGRESS);
        } else if (isInTabSwitcherMode()) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE);
        } else if (mNtpSearchBoxTranslation.y != 0) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.NTP_Y_TRANSLATION);
        } else {
            PhoneCaptureStateToken newSnapshotState = generateToolbarSnapshotState();
            @ToolbarSnapshotDifference
            int snapshotDifference =
                    PhoneCaptureStateToken.getAnyDifference(
                            mPhoneCaptureStateToken, newSnapshotState);
            if (snapshotDifference == ToolbarSnapshotDifference.NONE) {
                return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SNAPSHOT_SAME);
            } else {
                return CaptureReadinessResult.readyWithSnapshotDifference(snapshotDifference);
            }
        }
    }

    private PhoneCaptureStateToken generateToolbarSnapshotState() {
        UrlBarData urlBarData = mLocationBar.getUrlBarData();
        if (urlBarData == null) urlBarData = getToolbarDataProvider().getUrlBarData();
        StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
        @DrawableRes
        int securityIconResource =
                statusCoordinator == null
                        ? getToolbarDataProvider().getSecurityIconResource(false)
                        : statusCoordinator.getSecurityIconResource();

        VisibleUrlText visibleUrlText =
                new VisibleUrlText(
                        urlBarData.displayText, mLocationBar.getOmniboxVisibleTextPrefixHint());
        assumeNonNull(getTint());
        return new PhoneCaptureStateToken(
                getTint().getDefaultColor(),
                mTabCountSupplier == null ? 0 : mTabCountSupplier.get(),
                mButtonData,
                mVisualState,
                visibleUrlText,
                securityIconResource,
                assumeNonNull(mHomeButtonDisplay.getForegroundColor()),
                mHomeButtonDisplay.getVisibility() == View.VISIBLE,
                getMenuButtonCoordinator().isShowingUpdateBadge(),
                getToolbarDataProvider().isPaintPreview(),
                getProgressBar().getProgress(),
                mUnfocusedLocationBarLayoutWidth,
                mBrowserControlsStateProvider.getControlsPosition());
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
    public void onHomeButtonIsEnabledUpdate(boolean homeButtonEnabled) {
        mIsHomeButtonEnabled = homeButtonEnabled;
        updateButtonVisibility();
    }

    @Override
    public void onHomepageIsNonNtpUpdate(boolean isHomepageNonNtp) {
        mIsHomepageNonNtp = isHomepageNonNtp;
        updateButtonVisibility();
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        updateButtonVisibility();
    }

    @Override
    public void updateButtonVisibility() {
        if (mHomeButtonDisplay != null) {
            mHomeButtonDisplay.updateState(
                    mVisualState, mIsHomeButtonEnabled, mIsHomepageNonNtp, urlHasFocus());
        }
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        if (mOptionalButtonCoordinator != null) {
            mOptionalButtonCoordinator.setIconForegroundColor(tint);
        }

        // TODO(amaralp): Have the LocationBar listen to tint changes.
        if (mLocationBar != null) mLocationBar.updateVisualsForState();

        if (mLayoutUpdater != null) mLayoutUpdater.run();
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        assert mTextureCaptureMode != textureMode;
        mTextureCaptureMode = textureMode;
        if (mTextureCaptureMode) {
            if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
                updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, mVisualState);
            }
            if (!hideShadowForIncognitoNtp()
                    && !hideShadowForInterstitial()
                    && !hideShadowForRegularNtpTextureCapture()) {
                getToolbarShadow().setVisibility(VISIBLE);
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
            mPhoneCaptureStateToken = generateToolbarSnapshotState();
        }
    }

    private boolean hideShadowForRegularNtpTextureCapture() {
        return !isIncognitoBranded()
                && UrlUtilities.isNtpUrl(getToolbarDataProvider().getCurrentGurl())
                && mNtpSearchBoxScrollFraction < 1.f;
    }

    private void updateViewsForTabSwitcherMode() {
        setVisibility(mTabSwitcherState == TAB_SWITCHER ? View.INVISIBLE : View.VISIBLE);
        updateProgressBarVisibility();
        updateShadowVisibility();
    }

    private void updateProgressBarVisibility() {
        getProgressBar().setVisibility(mTabSwitcherState != STATIC_TAB ? INVISIBLE : VISIBLE);
    }

    @Override
    public void setContentAttached(boolean attached) {
        updateVisualsForLocationBarState();
    }

    @Override
    public void setTabSwitcherMode(boolean inTabSwitcherMode) {
        // On entering the tab switcher, set the focusability of the url bar to be false. This will
        // occur at the start of the enter event, and will later be reset to true upon finishing the
        // exit event.
        if (inTabSwitcherMode) {
            mLocationBar.setUrlBarFocusable(false);
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

        mTabSwitcherState = inTabSwitcherMode ? ENTERING_TAB_SWITCHER : EXITING_TAB_SWITCHER;

        // The width of location bar depends on mTabSwitcherState so layout request is needed. See
        // crbug.com/974745.
        ViewUtils.requestLayout(this, "ToolbarPhone.setTabSwitcherMode");

        finishAnimations();

        if (inTabSwitcherMode) {
            endUrlFocusAnimation();
            updateViewsForTabSwitcherMode();
        }

        // Since mTabSwitcherState has changed, we need to also check if mVisualState should
        // change.
        updateVisualsForLocationBarState();

        postInvalidateOnAnimation();
    }

    private void endUrlFocusAnimation() {
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.end();
            mUrlFocusLayoutAnimator = null;
        }
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
        }

        // The width of location bar depends on mTabSwitcherState so layout request is needed. See
        // crbug.com/974745.
        ViewUtils.requestLayout(this, "ToolbarPhone.onTabSwitcherTransitionFinished");
        finishAnimations();
        updateVisualsForLocationBarState();
        updateViewsForTabSwitcherMode();
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return super.shouldIgnoreSwipeGesture()
                || mUrlExpansionFraction > 0f
                || mNtpSearchBoxTranslation.y < 0f;
    }

    @Override
    public void setRequestFixedHeight(boolean requestFixedHeight) {
        if (requestFixedHeight) {
            ViewGroup.LayoutParams layoutParams = getLayoutParams();
            layoutParams.height = getHeight();
            setLayoutParams(layoutParams);
        } else {
            updateLayoutParamsForMultiline();
        }
    }

    private void populateUrlExpansionAnimatorSet(List<Animator> animators) {
        TraceEvent.begin("ToolbarPhone.populateUrlFocusingAnimatorSet");
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangeFractionProperty, 1f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        animators.add(animator);

        mLocationBar
                .getPhoneCoordinator()
                .populateFadeAnimation(animators, 0, URL_FOCUS_CHANGE_ANIMATION_DURATION_MS, 0);

        float density = getContext().getResources().getDisplayMetrics().density;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        float toolbarButtonTranslationX =
                MathUtils.flipSignIf(mUrlFocusTranslationX, isRtl) * density;

        // Hide the toolbar buttons immediately on the NTP when animating in the suggestions list to
        // avoid mixing horizontal and vertical motion; the suggestions translate in vertically.
        int toolbarButtonFadeDuration =
                animatingSuggestionsListOnNtp() ? 0 : URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS;
        animator = getMenuButtonCoordinator().getUrlFocusingAnimator(true);
        animator.setDuration(toolbarButtonFadeDuration);
        animator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animators.add(animator);

        View homeButton = mHomeButtonDisplay.getView();
        animator =
                ObjectAnimator.ofFloat(
                        homeButton,
                        TRANSLATION_X,
                        MathUtils.flipSignIf(-homeButton.getWidth() * density, isRtl));
        animator.setDuration(toolbarButtonFadeDuration);
        animator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animators.add(animator);

        if (getTabSwitcherButtonCoordinator() != null) {
            animator =
                    ObjectAnimator.ofFloat(
                            getTabSwitcherButtonCoordinator().getContainerView(),
                            TRANSLATION_X,
                            toolbarButtonTranslationX);
            animator.setDuration(toolbarButtonFadeDuration);
            animator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
            animators.add(animator);

            animator =
                    ObjectAnimator.ofFloat(
                            getTabSwitcherButtonCoordinator().getContainerView(), ALPHA, 0);
            animator.setDuration(toolbarButtonFadeDuration);
            animator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
            animators.add(animator);
        }

        if (getToolbarShadow() != null) {
            animator = ObjectAnimator.ofFloat(getToolbarShadow(), ALPHA, urlHasFocus() ? 0 : 1);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
            animators.add(animator);
        }
        TraceEvent.end("ToolbarPhone.populateUrlFocusingAnimatorSet");
    }

    private void populateUrlClearExpansionAnimatorSet(List<Animator> animators) {
        Animator animator = ObjectAnimator.ofFloat(this, mUrlFocusChangeFractionProperty, 0f);
        animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
        animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        animators.add(animator);

        animator = getMenuButtonCoordinator().getUrlFocusingAnimator(false);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animators.add(animator);

        animator = ObjectAnimator.ofFloat(mHomeButtonDisplay.getView(), TRANSLATION_X, 0);
        animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
        animator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animators.add(animator);

        if (getTabSwitcherButtonCoordinator() != null) {
            animator =
                    ObjectAnimator.ofFloat(
                            getTabSwitcherButtonCoordinator().getContainerView(), TRANSLATION_X, 0);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_TABSTACK_DELAY_MS);
            animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
            animators.add(animator);

            animator =
                    ObjectAnimator.ofFloat(
                            getTabSwitcherButtonCoordinator().getContainerView(), ALPHA, 1);
            animator.setDuration(URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS);
            animator.setStartDelay(URL_CLEAR_FOCUS_TABSTACK_DELAY_MS);
            animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
            animators.add(animator);
        }

        mLocationBar
                .getPhoneCoordinator()
                .populateFadeAnimation(
                        animators,
                        URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS,
                        URL_CLEAR_FOCUS_MENU_DELAY_MS,
                        1);

        if (isLocationBarShownInNtp() && mNtpSearchBoxScrollFraction == 0f) return;

        if (getToolbarShadow() != null) {
            animator = ObjectAnimator.ofFloat(getToolbarShadow(), ALPHA, 1);
            animator.setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS);
            animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
            animators.add(animator);
        }
    }

    @Override
    public void onUrlFocusChange(final boolean hasFocus) {
        super.onUrlFocusChange(hasFocus);

        // Unblock toolbar height when in focused mode, allowing the Omnibox to expand.
        // TODO(crbug.com/432311666): address following open questions
        // - this may overdraw the toolbar hairline, which has a fixed margin equal to
        //   toolbarHeight.
        // - need to notify TopControlStacker of toolbar height changes, so that other top controls
        //   are repositioned accordingly. Currently this would be bookmark bar (AL only) progress
        //   bar, and hairline.
        // - investigate what else needs to be done to make the WRAP_CONTENT work well as the
        //   default / static setting (likely leading to elimination of `toolbar_height_no_shadow`
        //   dimension).
        if (OmniboxFeatures.allowMultilineEditField()
                || ChromeFeatureList.sAndroidBottomToolbarV2.isEnabled()) {
            updateLayoutParamsForMultiline();
        }

        if (ChromeFeatureList.sAndroidBottomToolbarV2.isEnabled()) {
            int verticalPadding = hasFocus ? mVerticalPaddingWhenFocused : 0;
            setPaddingRelative(
                    getPaddingStart(),
                    verticalPadding + mTopPaddingForEdgeToEdgeNtp,
                    getPaddingEnd(),
                    verticalPadding);
            updateBackgroundHairline(hasFocus, AutocompleteRequestType.SEARCH);
        }

        // If the refactored animations are enabled, we want to update the bg only after we've
        // started the delayed transition in order to grab the correct starting properties.
        if (!ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            updateBackground(hasFocus);
        }
        updateLocationBarForNtp(mVisualState, urlHasFocus());
        ToggleTabStackButtonCoordinator tabSwitcherButtonCoordinator =
                getTabSwitcherButtonCoordinator();
        assumeNonNull(tabSwitcherButtonCoordinator);
        tabSwitcherButtonCoordinator.getContainerView().setClickable(!hasFocus);
        mHomeButtonDisplay.setClickable(!hasFocus);
        triggerUrlFocusAnimation(hasFocus);
    }

    private void updateLayoutParamsForMultiline() {
        var params = getLayoutParams();
        params.height =
                urlHasFocus()
                        ? LayoutParams.WRAP_CONTENT
                        : getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                                + mTopPaddingForEdgeToEdgeNtp;
        setLayoutParams(params);
    }

    private boolean animatingSuggestionsListOnNtp() {
        return OmniboxFeatures.shouldAnimateSuggestionsListAppearance()
                && getToolbarDataProvider().getNewTabPageDelegate().isLocationBarShown();
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
        if (mOptionalButtonAnimationRunning) {
            assumeNonNull(mOptionalButtonCoordinator);
            mOptionalButtonCoordinator.cancelTransition();
        }
        if (hasFocus && mBrandColorTransitionActive) {
            assumeNonNull(mBrandColorTransitionAnimation);
            mBrandColorTransitionAnimation.cancel();
        }

        mUrlFocusChangeInProgress = true;
        // Hide the optional button immediately when animating in the suggestions list (since other
        // toolbar buttons are also hidden immediately) or restore it when omnibox focus is lost.
        if (animatingSuggestionsListOnNtp()) {
            ButtonData copy = mButtonData;
            updateOptionalButton(hasFocus ? null : mButtonData);
            mButtonData = copy;
        }

        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            createAndRunFocusAnimatorRefactored(hasFocus);
        } else {
            createAndRunFocusAnimatorLegacy(hasFocus, shouldShowKeyboard);
        }
        if (!hasFocus || mLocationBar.shouldShortCircuitFocusAnimation(true)) {
            TraceEvent.instant("ToolbarPhone.ShortCircuitUnfocusAnimation");
            endUrlFocusAnimation();
        }
        TraceEvent.end("ToolbarPhone.triggerUrlFocusAnimation");
    }

    private void createAndRunFocusAnimatorLegacy(boolean hasFocus, boolean shouldShowKeyboard) {
        List<Animator> animators = new ArrayList<>();
        if (hasFocus) {
            populateUrlExpansionAnimatorSet(animators);
        } else {
            populateUrlClearExpansionAnimatorSet(animators);
        }
        mUrlFocusLayoutAnimator = new AnimatorSet();
        mUrlFocusLayoutAnimator.playTogether(animators);

        mUrlFocusLayoutAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animation) {
                        if (!hasFocus) {
                            mDisableLocationBarRelayout = true;
                        } else {
                            mLayoutLocationBarInFocusedMode = true;
                            ViewUtils.requestLayout(
                                    ToolbarPhone.this,
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
                            ViewUtils.requestLayout(
                                    ToolbarPhone.this,
                                    "ToolbarPhone.triggerUrlFocusAnimation.CancelAwareAnimatorListener.onEnd");
                        }
                        mLocationBar.finishUrlFocusChange(hasFocus, shouldShowKeyboard);
                        mUrlFocusChangeInProgress = false;
                    }
                });
        mUrlFocusLayoutAnimator.start();
    }

    private class BackgroundDrawableTransition extends Transition {
        private static final String PROPNAME_TOOLBAR_COLOR =
                "BackgroundDrawableTransition:toolbarColor";
        private static final String PROPNAME_LOCATION_BAR_COLOR =
                "BackgroundDrawableTransition:locationBarColor";
        private static final String PROPNAME_LOCATION_BAR_CORNER_RADIUS =
                "BackgroundDrawableTransition:locationBarCornerRadius";

        private static final String[] sTransitionProperties = {
            PROPNAME_TOOLBAR_COLOR, PROPNAME_LOCATION_BAR_COLOR, PROPNAME_LOCATION_BAR_CORNER_RADIUS
        };

        public BackgroundDrawableTransition() {
            // Ensure we only create one transition.
            addTarget(ToolbarPhone.this);
        }

        private void captureValues(TransitionValues transitionValues) {
            assert transitionValues.view == ToolbarPhone.this : "Unexpected transition target.";
            transitionValues.values.put(PROPNAME_TOOLBAR_COLOR, mToolbarBackground.getColor());

            GradientDrawable locationBarBackgroundGradientDrawable =
                    getActiveLocationBarGradientDrawable();
            if (locationBarBackgroundGradientDrawable != null) {
                ColorStateList colorStateList = locationBarBackgroundGradientDrawable.getColor();
                if (colorStateList != null) {
                    transitionValues.values.put(
                            PROPNAME_LOCATION_BAR_COLOR, colorStateList.getDefaultColor());
                }

                transitionValues.values.put(
                        PROPNAME_LOCATION_BAR_CORNER_RADIUS,
                        locationBarBackgroundGradientDrawable.getCornerRadius());
            }
        }

        private @Nullable GradientDrawable getActiveLocationBarGradientDrawable() {
            if (mActiveLocationBarBackground == mLocationBarBackground) {
                return mLocationBarBackground.getBackgroundGradient();
            } else if (mActiveLocationBarBackground == mNtpFakeboxBackground) {
                return mNtpFakeboxBackground;
            }
            return null;
        }

        @Override
        public void captureStartValues(TransitionValues transitionValues) {
            captureValues(transitionValues);
        }

        @Override
        public void captureEndValues(TransitionValues transitionValues) {
            captureValues(transitionValues);
        }

        @Nullable
        @Override
        public Animator createAnimator(
                @NonNull ViewGroup sceneRoot,
                @Nullable TransitionValues startValues,
                @Nullable TransitionValues endValues) {
            if (startValues == null || endValues == null) return null;
            assert startValues.view == ToolbarPhone.this : "Unexpected transition target.";
            assert endValues.view == ToolbarPhone.this : "Unexpected transition target.";
            List<Animator> animatorList = new ArrayList<>();

            Object toolbarStartColor = startValues.values.get(PROPNAME_TOOLBAR_COLOR);
            Object toolbarEndColor = endValues.values.get(PROPNAME_TOOLBAR_COLOR);
            if (toolbarStartColor != null
                    && toolbarEndColor != null
                    && Color.alpha((int) toolbarEndColor) != 0) {
                ObjectAnimator toolbarBackgroundAnimator =
                        ObjectAnimator.ofArgb(
                                mToolbarBackground,
                                "color",
                                (int) toolbarStartColor,
                                (int) toolbarEndColor);
                toolbarBackgroundAnimator.addUpdateListener(
                        valueAnimator ->
                                notifyToolbarColorChanged((int) valueAnimator.getAnimatedValue()));
                animatorList.add(toolbarBackgroundAnimator);
            }

            Object locationBarStartColor = startValues.values.get(PROPNAME_LOCATION_BAR_COLOR);
            Object locationBarEndColor = endValues.values.get(PROPNAME_LOCATION_BAR_COLOR);
            GradientDrawable locationBarBackgroundGradientDrawable =
                    getActiveLocationBarGradientDrawable();
            if (locationBarStartColor != null
                    && locationBarEndColor != null
                    && locationBarBackgroundGradientDrawable != null) {
                ValueAnimator animator = ValueAnimator.ofFloat(0, 1);
                animator.addUpdateListener(
                        valueAnimator -> {
                            float fraction = valueAnimator.getAnimatedFraction();
                            @ColorInt
                            int newColor =
                                    ColorUtils.blendColorsMultiply(
                                            (int) locationBarStartColor,
                                            (int) locationBarEndColor,
                                            fraction);
                            locationBarBackgroundGradientDrawable.setColor(newColor);
                        });
                animatorList.add(animator);
            }

            Object locationBarStartCornerRadius =
                    startValues.values.get(PROPNAME_LOCATION_BAR_CORNER_RADIUS);
            Object locationBarEndCornerRadius =
                    endValues.values.get(PROPNAME_LOCATION_BAR_CORNER_RADIUS);
            if (locationBarStartCornerRadius != null
                    && locationBarEndCornerRadius != null
                    && locationBarBackgroundGradientDrawable != null) {
                animatorList.add(
                        ObjectAnimator.ofFloat(
                                locationBarBackgroundGradientDrawable,
                                "cornerRadius",
                                (float) locationBarStartCornerRadius,
                                (float) locationBarEndCornerRadius));
            }

            AnimatorSet animatorSet = new AnimatorSet();
            animatorSet.playTogether(animatorList);
            return animatorSet;
        }

        @Override
        public String[] getTransitionProperties() {
            return sTransitionProperties;
        }
    }

    // TODO(crbug.com/430347234): Refine animations to match the spec.
    private void createAndRunFocusAnimatorRefactored(boolean hasFocus) {
        int toolbarBtnTransitionDuration =
                hasFocus && animatingSuggestionsListOnNtp()
                        ? 0
                        : URL_FOCUS_TOOLBAR_BUTTONS_DURATION_MS;
        TransitionSet buttonsTransition =
                new TransitionSet()
                        .addTransition(
                                new Slide(Gravity.START).addTarget(mHomeButtonDisplay.getView()))
                        .addTransition(new Slide(Gravity.END).addTarget(mToolbarButtonsContainer))
                        .addTransition(new Fade().addTarget(mToolbarButtonsContainer))
                        .setDuration(toolbarBtnTransitionDuration)
                        .setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);

        // We explicitly #setResizeClip(true) for the location bar as it contains text.
        ChangeBounds changeBoundsWithText = new ChangeBounds();
        changeBoundsWithText.setResizeClip(true);
        TransitionSet locationBarTransition =
                new TransitionSet()
                        .addTransition(
                                changeBoundsWithText.addTarget(mLocationBar.getContainerView()))
                        .addTransition(
                                new ChangeBounds().addTarget(mActiveLocationBarBackgroundView))
                        .addTransition(
                                new ChangeTransform()
                                        .addTarget(mLocationBar.getContainerView())
                                        .addTarget(mActiveLocationBarBackgroundView))
                        .addTransition(new BackgroundDrawableTransition())
                        .setDuration(URL_FOCUS_CHANGE_ANIMATION_DURATION_MS)
                        .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        TransitionSet transition =
                new TransitionSet()
                        .addTransition(buttonsTransition)
                        .addTransition(locationBarTransition);

        transition.addListener(
                new TransitionListenerAdapter() {
                    @Override
                    public void onTransitionStart(Transition transition) {
                        super.onTransitionStart(transition);
                        onFocusTransitionStart();
                    }

                    @Override
                    public void onTransitionCancel(Transition transition) {
                        super.onTransitionCancel(transition);
                        mUrlFocusChangeInProgress = false;
                    }

                    @Override
                    public void onTransitionEnd(Transition transition) {
                        super.onTransitionEnd(transition);
                        onFocusTransitionEnd(hasFocus);
                    }
                });

        TransitionManager.beginDelayedTransition(this, transition);

        int toolbarBtnsVis = hasFocus ? INVISIBLE : VISIBLE;
        int homeBtnVis =
                mHomeButtonDisplay.getVisibility() != GONE
                        ? toolbarBtnsVis
                        : mHomeButtonDisplay.getVisibility();
        int locationBarLeftMargin =
                hasFocus
                        ? getFocusedLocationBarLeftMargin(0, true)
                        : mUnfocusedLocationBarLayoutLeft;
        int locationBarWidth =
                hasFocus
                        ? getFocusedLocationBarWidth(getWidth(), 0, true)
                        : mUnfocusedLocationBarLayoutWidth;

        mToolbarButtonsContainer.setVisibility(toolbarBtnsVis);
        mHomeButtonDisplay.getView().setVisibility(homeBtnVis);
        getToolbarShadow().setVisibility(toolbarBtnsVis);
        MarginLayoutParams layoutParams =
                mLocationBar.getPhoneCoordinator().getMarginLayoutParams();
        layoutParams.leftMargin = locationBarLeftMargin;
        layoutParams.width = locationBarWidth;
        mLocationBar.getContainerView().setLayoutParams(layoutParams);
        // TODO (crbug.com/430347234): Should be handled by LocationBarCoordinator. Not setting it
        // here causes this container to be visible in first frame during unfocus.
        mLocationBar.setUrlActionContainerVisibility(hasFocus);

        float focusChangeFraction = hasFocus ? 1f : 0f;
        if (isLocationBarShownInNtp()) {
            NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
            ntpDelegate.setUrlFocusChangeAnimationPercent(focusChangeFraction);
            updateLocationBarNtpOffset(
                    /* expanded= */ hasFocus || mNtpSearchBoxScrollFraction == 1.f);
        }
        updateBackground(hasFocus);

        // TODO(crbug.com/425817689): In the end state of the refactored animations, we don't want
        //  to rely on the interpolation methods that will be called by #setUrlFocusChangeFraction
        //  (namely #invokeTransition). We instead want to directly set the appropriate end state,
        //  like we do with the button visibility and location bar layout params above.
        setUrlFocusChangeFraction(focusChangeFraction);

        // Set after the fraction update, since this depends on the updated fraction.
        updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, mVisualState);
    }

    private void updateLocationBarNtpOffset(boolean expanded) {
        if (!isLocationBarShownInNtp()) return;

        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        ntpDelegate.getSearchBoxBounds(mNtpSearchBoxBounds, mNtpSearchBoxTranslation);
        if (expanded) {
            mLocationBarBackgroundNtpOffset.set(0, 0, 0, 0);
            mLocationBarNtpOffsetLeft = 0;
            mLocationBarNtpOffsetRight = 0;
            mLocationBar.getPhoneCoordinator().setTranslationY(0);
        } else {
            int left = mNtpSearchBoxBounds.left - mLocationBarBackgroundBounds.left;
            int right = mNtpSearchBoxBounds.right - mLocationBarBackgroundBounds.right;
            float translationY = getLocationBarTranslationY();

            mLocationBarBackgroundNtpOffset.set(
                    left, (int) translationY, right, (int) translationY);
            mLocationBarBackgroundNtpOffset.inset(0, (int) getNtpTransitionVerticalInset());
            mLocationBarNtpOffsetLeft =
                    mNtpSearchBoxBounds.left - getFocusedLeftPositionOfLocationBarBackground();
            mLocationBarNtpOffsetRight =
                    mNtpSearchBoxBounds.right - getFocusedRightPositionOfLocationBarBackground();
            mLocationBar.getPhoneCoordinator().setTranslationY(translationY);
        }
    }

    private void onFocusTransitionStart() {
        if (mLocationBar.isDestroyed()) return;
        // Ensure visibility during the animation.
        if (isLocationBarShownInNtp()) {
            // Needed for the NTP transition, to prevent the real location bar from getting clipped
            // while outside of its parent's bounds during the translation to/from the fakebox's
            // position.
            setAncestorsShouldClipChildren(false);
            setClipToPadding(false);

            NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
            ntpDelegate.setSearchBoxAlpha(0.f);
        }
        mLocationBar.getPhoneCoordinator().setAlpha(1.f);
        mActiveLocationBarBackgroundView.setAlpha(1.f);
    }

    private void onFocusTransitionEnd(boolean hasFocus) {
        if (mLocationBar.isDestroyed()) return;

        mUrlFocusChangeInProgress = false;
        mRefactoredNtpStartingOffset = 0.f;

        if (isLocationBarShownInNtp()) {
            setAncestorsShouldClipChildren(true);
            setClipToPadding(true);

            if (mNtpSearchBoxScrollFraction != 1.f) {
                NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
                ntpDelegate.setSearchBoxAlpha(1.f);
                if (!hasFocus) {
                    // When unfocusing, the NTP state may be reset. If that is the case, we need to
                    // update the scroll translation here.
                    updateLocationBarTranslationOnScroll();
                    mLocationBar.getPhoneCoordinator().setAlpha(0.f);
                    mActiveLocationBarBackgroundView.setAlpha(0.f);
                }
            }
        }
        mLocationBar.finishUrlFocusChange(hasFocus, hasFocus);
    }

    /**
     * The refactored flow uses a {@link TransitionManager} to handle the its animations. Evidently,
     * when {@link TransitionManager#endTransitions} is called, it immediately removes all pending
     * transitions, effectively allowing the next update cycle to run as expected, without kicking
     * off the delayed transitions. As a result, the {@link TransitionListener} events
     * (onTransitionStart, etc.) also are skipped. Our implementation relies on these events being
     * called, so manually call them when ending the focus transitions early.
     *
     * @param hasFocus {@code True} if the URL bar has focus at the end of the transition.
     */
    private void endFocusTransition(boolean hasFocus) {
        onFocusTransitionStart();
        TransitionManager.endTransitions(this);
        onFocusTransitionEnd(hasFocus);
    }

    // ToolbarDataProvider.Observer implementation.

    @Override
    public void onIncognitoStateChanged() {
        updateRippleBackground();
    }

    @Override
    public void setTabCountSupplier(ObservableSupplier<Integer> tabCountSupplier) {
        mTabCountSupplier = tabCountSupplier;
    }

    @Override
    public void onTabContentViewChanged() {
        super.onTabContentViewChanged();
        updateNtpAnimationState();
        updateVisualsForLocationBarState();
    }

    @Override
    public void onDidFirstVisuallyNonEmptyPaint() {
        super.onDidFirstVisuallyNonEmptyPaint();
        if (mIsInLoadingPhaseFromNtpToWebpage) {
            // Updates toolbar and location bar's color to the default color when transitioning from
            // NTP to other webpages before {@link ToolbarPhone#onPrimaryColorChanged(boolean)}
            // is triggered. TODO(crbug.com/323888159): This code was created to deal with the
            // case when the navigating webpage does not have the "theme-color" meta element or does
            // not have its own brand color. In this situation, the function and {@link
            // ToolbarPhone#onPrimaryColorChanged(boolean)} won't be triggered. This solution has
            // limitations when dealing with webpages that use a brand color, as there may be a few
            // frames with incorrect colors before the correct brand color is loaded. This code will
            // be removed once a better solution is developed.
            onPrimaryColorChanged(/* shouldAnimate= */ true);
        }
    }

    @Override
    public void onTabOrModelChanged() {
        super.onTabOrModelChanged();
        updateNtpAnimationState();
        updateVisualsForLocationBarState();
    }

    /** Called when the tab model changes. */
    private void updateRippleBackground() {
        var toolbarIconRippleId =
                isIncognitoBranded()
                        ? R.drawable.default_icon_background_baseline
                        : R.drawable.default_icon_background;
        var omniboxIconRippleId =
                isIncognitoBranded()
                        ? R.drawable.search_box_icon_background_baseline
                        : R.drawable.search_box_icon_background;
        mHomeButtonDisplay.setBackgroundResource(toolbarIconRippleId);
        getMenuButtonCoordinator().updateButtonBackground(toolbarIconRippleId);
        mLocationBar.updateButtonBackground(omniboxIconRippleId);
    }

    private static boolean isVisualStateValidForBrandColorTransition(@VisualState int state) {
        return state == VisualState.NORMAL
                || state == VisualState.BRAND_COLOR
                || state == VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO;
    }

    @Override
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        super.onPrimaryColorChanged(shouldAnimate);
        if (mBrandColorTransitionActive) {
            assumeNonNull(mBrandColorTransitionAnimation);
            mBrandColorTransitionAnimation.end();
        }

        final @ColorInt int initialColor = mToolbarBackground.getColor();
        final @ColorInt int finalColor =
                isLocationBarShownInGeneralNtp()
                        ? mToolbarBackgroundColorForNtp
                        : getToolbarDataProvider().getPrimaryColor();
        if (initialColor == finalColor) return;

        final @ColorInt int initialLocationBarColor =
                getLocationBarColorForToolbarColor(initialColor);

        // When the webpage finishes loading during the NTP phase, the process should halt at this
        // point because the tab's color is updated, and the initial color of the location bar is
        // established for the upcoming navigation animation.
        mIsInLoadingPhaseFromNtpToWebpage = false;

        final @ColorInt int finalLocationBarColor = getLocationBarColorForToolbarColor(finalColor);

        // Ignore theme color changes while the omnibox is focused, since we want a standard,
        // app-defined color for the toolbar in this scenario, not the site's color. We'll
        // transition back to the site's color on unfocus.
        if (urlHasFocus() || !isVisualStateValidForBrandColorTransition(mVisualState)) return;

        if (!shouldAnimate) {
            updateToolbarBackground(finalColor);
            return;
        }

        mBrandColorTransitionAnimation =
                ValueAnimator.ofFloat(0, 1).setDuration(THEME_COLOR_TRANSITION_DURATION);
        mBrandColorTransitionAnimation.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        mBrandColorTransitionAnimation.addUpdateListener(
                (ValueAnimator animation) -> {
                    float fraction = animation.getAnimatedFraction();
                    updateToolbarBackground(
                            ColorUtils.blendColorsMultiply(initialColor, finalColor, fraction));
                    updateModernLocationBarColor(
                            ColorUtils.blendColorsMultiply(
                                    initialLocationBarColor, finalLocationBarColor, fraction));
                });
        mBrandColorTransitionAnimation.addListener(
                new CancelAwareAnimatorListener() {
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

        invokeTransition(/* resetNtpTransition= */ true, /* skipUrlExpansion= */ false);
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
        mDefaultSearchEngineChangedRunnable =
                () -> {
                    updateVisualsForLocationBarState();
                    updateNtpAnimationState();
                };
        handler.post(mDefaultSearchEngineChangedRunnable);
    }

    @Override
    public void handleFindLocationBarStateChange(boolean showing) {
        setVisibility(
                showing
                        ? View.GONE
                        : mTabSwitcherState == STATIC_TAB ? View.VISIBLE : View.INVISIBLE);
    }

    @VisibleForTesting
    boolean isLocationBarShownInNtp() {
        return getToolbarDataProvider().getNewTabPageDelegate().isLocationBarShown();
    }

    boolean isLocationBarShownInGeneralNtp() {
        return !isIncognito() && UrlUtilities.isNtpUrl(getToolbarDataProvider().getCurrentGurl());
    }

    private boolean isLocationBarCurrentlyShown() {
        return !isLocationBarShownInNtp() || mUrlExpansionFraction > 0;
    }

    /**
     * @return Whether the toolbar shadow should be drawn.
     */
    @Override
    protected boolean shouldDrawShadow() {
        // TODO(twellington): Move this shadow state information to ToolbarDataProvider and show
        // shadow when incognito NTP is scrolled.
        return super.shouldDrawShadow()
                && mTabSwitcherState == STATIC_TAB
                && !hideShadowForIncognitoNtp()
                && !hideShadowForInterstitial()
                && getVisibility() == View.VISIBLE;
    }

    @Override
    public void onToEdgeChange(int newTopPadding) {
        // The status bar hides when the newTopInset value is greater than zero. When the New Tab
        // Page (NTP) transitions to an edge-to-edge display at the top, we add the original status
        // bar height as the top padding to the toolbar. This ensures the toolbar stays in its
        // original screen position, and the entire top area (status bar and toolbar) is rendered
        // with the toolbar's color.
        mTopPaddingForEdgeToEdgeNtp = newTopPadding;
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) getLayoutParams();
        int height =
                getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + newTopPadding;
        marginLayoutParams.height = height;

        setPaddingRelative(
                getPaddingStart(),
                mTopPaddingForEdgeToEdgeNtp,
                getPaddingEnd(),
                getPaddingBottom());
    }

    private boolean hideShadowForIncognitoNtp() {
        return isIncognitoBranded()
                && UrlUtilities.isNtpUrl(getToolbarDataProvider().getCurrentGurl());
    }

    private boolean hideShadowForInterstitial() {
        return getToolbarDataProvider() != null
                && getToolbarDataProvider().getTab() != null
                && getToolbarDataProvider().getTab().isShowingErrorPage();
    }

    private @VisualState int computeVisualState() {
        if (isLocationBarShownInNtp()) return VisualState.NEW_TAB_NORMAL;
        if (isLocationBarShownInGeneralNtp()) {
            return VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO;
        }
        if (isIncognitoBranded()) return VisualState.INCOGNITO;
        if (getToolbarDataProvider().isUsingBrandColor()) return VisualState.BRAND_COLOR;
        return VisualState.NORMAL;
    }

    /**
     * @return The color that progress bar should use.
     */
    private @ColorInt int getProgressBarColor() {
        return getToolbarDataProvider().getPrimaryColor();
    }

    /**
     * Update the appearance (logo background, search text's color and style) of the location bar
     * based on the state of the current page. For NTP, while not on the focus state, the real
     * search box's search text has a particular color and style. The style would be
     * "google-sans-medium" and the color would be colorOnSurface. When being in light mode, there
     * is also a round white background for the G logo. For other situations such as browser tabs,
     * and focus state, the real search box will stay the same.
     *
     * @param visualState The Visual State of the current page.
     * @param hasFocus True if the current page is the focus state.
     */
    @VisibleForTesting
    void updateLocationBarForNtp(@VisualState int visualState, boolean hasFocus) {
        // Detect whether state has changed and update only when that happens.
        boolean prevIsNtpWithUnfocusedRealOmnibox = mIsNtpWithUnfocusedRealOmnibox;

        // Check whether the current page is NTP (the focus state is not included) and the
        // real omnibox is pinned on the top of the screen. We need to make sure the real omnibox is
        // visible here to forbid the situation that the background of the G logo shows and then
        // vanishes during the un-focus animation(from focus state to NTP).
        boolean isNtpShowingWithRealOmnibox =
                visualState == VisualState.NEW_TAB_NORMAL && mNtpSearchBoxScrollFraction > 0;
        mIsNtpWithUnfocusedRealOmnibox = isNtpShowingWithRealOmnibox && !hasFocus;

        if (mIsNtpWithUnfocusedRealOmnibox == prevIsNtpWithUnfocusedRealOmnibox) {
            return;
        }

        if (mIsNtpWithUnfocusedRealOmnibox) {
            updateLocationBarForNtpImpl(
                    /* useDefaultUrlBarAndUrlActionContainerAppearance= */ false);
        } else {
            // Restore the appearance of the real search box when transitioning from NTP to other
            // pages.
            updateLocationBarForNtpImpl(
                    /* useDefaultUrlBarAndUrlActionContainerAppearance= */ true);
        }
    }

    /**
     * Update the appearance (logo background, search text's color and style) of the location bar
     * based on the value provided.
     *
     * @param useDefaultUrlBarAndUrlActionContainerAppearance Whether to use the default typeface
     *     and color for the search text in the search box and use the default end margin for the
     *     url action container in the search box. If not we will use specific settings for NTP's
     *     un-focus state.
     */
    private void updateLocationBarForNtpImpl(
            boolean useDefaultUrlBarAndUrlActionContainerAppearance) {
        mLocationBar.updateUrlBarHintTextColor(useDefaultUrlBarAndUrlActionContainerAppearance);
        mLocationBar.updateUrlActionContainerEndMargin(
                useDefaultUrlBarAndUrlActionContainerAppearance);
    }

    private void updateVisualsForLocationBarState() {
        TraceEvent.begin("ToolbarPhone.updateVisualsForLocationBarState");

        @VisualState int newVisualState = computeVisualState();
        updateLocationBarForNtp(newVisualState, urlHasFocus());

        if (newVisualState == VisualState.NEW_TAB_NORMAL) {
            mHomeButtonDisplay.setAccessibilityTraversalBefore(R.id.toolbar_buttons);
        } else {
            mHomeButtonDisplay.setAccessibilityTraversalBefore(View.NO_ID);
        }

        // If we are navigating to or from a brand color, allow the transition animation
        // to run to completion as it will handle the triggering this path again and committing
        // the proper visual state when it finishes.  Brand color transitions are only valid
        // between normal non-incognito pages and brand color pages, so if the visual states
        // do not match then cancel the animation below.
        if (mBrandColorTransitionActive
                && isVisualStateValidForBrandColorTransition(mVisualState)
                && isVisualStateValidForBrandColorTransition(newVisualState)) {
            TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
            return;
        } else if (mBrandColorTransitionAnimation != null
                && mBrandColorTransitionAnimation.isRunning()) {
            mBrandColorTransitionAnimation.end();
        }

        boolean visualStateChanged = mVisualState != newVisualState;

        @ColorInt
        int currentPrimaryColor =
                isLocationBarShownInGeneralNtp()
                        ? mToolbarBackgroundColorForNtp
                        : getToolbarDataProvider().getPrimaryColor();
        @ColorInt int themeColorForProgressBar = getProgressBarColor();

        // If The page is native force the use of the standard theme for the progress bar.
        if (getToolbarDataProvider() != null
                && getToolbarDataProvider().getTab() != null
                && getToolbarDataProvider().getTab().isNativePage()) {
            @VisualState
            int visualState = isIncognitoBranded() ? VisualState.INCOGNITO : VisualState.NORMAL;
            themeColorForProgressBar = getToolbarColorForVisualState(visualState);
        }

        if (mVisualState == VisualState.BRAND_COLOR && !visualStateChanged) {
            boolean unfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
            if (unfocusedLocationBarUsesTransparentBg != mUnfocusedLocationBarUsesTransparentBg) {
                visualStateChanged = true;
            } else {
                updateToolbarBackgroundFromState(VisualState.BRAND_COLOR);
                getProgressBar().setThemeColor(themeColorForProgressBar, isIncognitoBranded());
            }
        }

        startLoadingPhaseFromNtpToWebpage(newVisualState);

        mVisualState = newVisualState;
        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            updateLocationBarBackgroundBounds(mLocationBarBackgroundBounds, newVisualState);
        }

        mHomeButtonDisplay.updateState(
                mVisualState, mIsHomeButtonEnabled, mIsHomepageNonNtp, urlHasFocus());

        // Refresh the toolbar texture.
        if ((mVisualState == VisualState.BRAND_COLOR || visualStateChanged)
                && mLayoutUpdater != null) {
            mLayoutUpdater.run();
        }
        updateShadowVisibility();
        invokeTransition(
                /* resetNtpTransition= */ false,
                /* skipUrlExpansion= */ ChromeFeatureList.sToolbarPhoneAnimationRefactor
                        .isEnabled());

        // This exception is to prevent early change of theme color when exiting the tab switcher
        // since currently visual state does not map correctly to tab switcher state. See
        // https://crbug.com/832594 for more info.
        if (mTabSwitcherState != EXITING_TAB_SWITCHER) {
            updateToolbarBackgroundFromState(mVisualState);
        }

        if (!visualStateChanged) {
            TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
            return;
        }

        mUnfocusedLocationBarUsesTransparentBg = false;
        getProgressBar().setThemeColor(themeColorForProgressBar, isIncognitoBranded());

        if (mVisualState == VisualState.BRAND_COLOR) {
            mUnfocusedLocationBarUsesTransparentBg =
                    !ColorUtils.shouldUseOpaqueTextboxBackground(currentPrimaryColor);
        }

        updateModernLocationBarColor(getLocationBarColorForToolbarColor(currentPrimaryColor));

        mLocationBar.updateVisualsForState();

        getMenuButtonCoordinator().setVisibility(true);
        TraceEvent.end("ToolbarPhone.updateVisualsForLocationBarState");
    }

    /**
     * Initiates the loading phase when transitioning from the NTP to a webpage. When the old visual
     * state is either VisualState.NEW_TAB_NORMAL or VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO, and
     * the new visual state updates to VisualState.NORMAL, and the current page is NTP, this
     * indicates the beginning of the process of navigating from the New Tab Page to other webpages.
     * This condition is unique because in other scenarios, the visual state does not change in this
     * manner.
     */
    private void startLoadingPhaseFromNtpToWebpage(@VisualState int newVisualState) {
        boolean isStartLoadingPhaseFromNtpToWebpage =
                (mVisualState == VisualState.NEW_TAB_NORMAL
                                || mVisualState == VisualState.NEW_TAB_SEARCH_ENGINE_NO_LOGO)
                        && newVisualState == VisualState.NORMAL;
        boolean hasVisibleNtp = getToolbarDataProvider().getNewTabPageDelegate().wasShowingNtp();
        if (isStartLoadingPhaseFromNtpToWebpage && hasVisibleNtp) {
            mIsInLoadingPhaseFromNtpToWebpage = true;
        }
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    private void initializeOptionalButton() {
        if (mOptionalButtonCoordinator == null) {
            ViewStub optionalButtonStub = findViewById(R.id.optional_button_stub);

            if (optionalButtonStub == null) {
                return;
            }

            optionalButtonStub.setLayoutResource(R.layout.optional_button_layout);

            View optionalButton = optionalButtonStub.inflate();

            BooleanSupplier isAnimationAllowedPredicate =
                    new BooleanSupplier() {
                        @Override
                        public boolean getAsBoolean() {
                            boolean transitioningAwayFromLocationBarInNtp =
                                    getToolbarDataProvider()
                                            .getNewTabPageDelegate()
                                            .transitioningAwayFromLocationBar();

                            return mTabSwitcherState == STATIC_TAB
                                    && !mUrlFocusChangeInProgress
                                    && !urlHasFocus()
                                    && !transitioningAwayFromLocationBarInNtp;
                        }
                    };

            assert mUserEducationHelper != null
                    : "initialize(...) must be called on the Toolbar first.";
            mOptionalButtonCoordinator =
                    new OptionalButtonCoordinator(
                            optionalButton,
                            () -> mUserEducationHelper,
                            /* transitionRoot= */ mToolbarButtonsContainer,
                            isAnimationAllowedPredicate,
                            mTrackerSupplier);

            // Set the button's background to the same color as the URL bar background. This color
            // is only used when showing dynamic actions.
            mOptionalButtonCoordinator.setBackgroundColorFilter(mCurrentLocationBarColor);
            // Set the button's foreground color to the same color as other toolbar icons. This
            // color is not used on icons that don't support tinting (e.g. user profile pic).
            mOptionalButtonCoordinator.setIconForegroundColor(getTint());
            mOptionalButtonCoordinator.setOnBeforeHideTransitionCallback(
                    () -> mLayoutLocationBarWithoutExtraButton = true);

            mOptionalButtonCoordinator.setTransitionStartedCallback(
                    transitionType -> {
                        TraceEvent.startAsync(
                                "OptionalButtonCoordinator.TransitionRunning", transitionType);
                        if (!mOptionalButtonAnimationRunning) {
                            mOptionalButtonAnimationStartTimeMs = TimeUtils.elapsedRealtimeMillis();
                        }
                        mOptionalButtonAnimationRunning = true;
                        keepControlsShownForAnimation();

                        switch (transitionType) {
                            case TransitionType.COLLAPSING_ACTION_CHIP:
                            case TransitionType.HIDING:
                                mLayoutLocationBarWithoutExtraButton = true;
                                ViewUtils.requestLayout(
                                        this,
                                        "ToolbarPhone.initializeOptionalButton.mOptionalButton.setTransitionStartedCallback");
                                break;
                            case TransitionType.EXPANDING_ACTION_CHIP:
                            case TransitionType.SHOWING:
                                mDisableLocationBarRelayout = true;
                                break;
                        }
                    });
            mOptionalButtonCoordinator.setTransitionFinishedCallback(
                    transitionType -> {
                        TraceEvent.finishAsync(
                                "OptionalButtonCoordinator.TransitionRunning", transitionType);
                        // If we are done expanding the transition chip then don't re-enable hiding
                        // browser controls, as we'll begin the collapse transition soon.
                        if (transitionType == TransitionType.EXPANDING_ACTION_CHIP) {
                            return;
                        }

                        mLayoutLocationBarWithoutExtraButton = false;
                        mDisableLocationBarRelayout =
                                ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled();
                        mOptionalButtonAnimationRunning = false;
                        allowBrowserControlsHide();
                        if (mLayoutUpdater != null) mLayoutUpdater.run();
                        ViewUtils.requestLayout(
                                this,
                                "ToolbarPhone.initializeOptionalButton.mOptionalButton.setTransitionFinishedCallback");
                    });

            mHomeButtonDisplay.setOnKeyListener(
                    new KeyboardNavigationListener() {
                        @Override
                        public @Nullable View getNextFocusForward() {
                            // If the url_bar is outside the toolbar's bounds, optional button is
                            // the next button to go in order to bypass the url_bar when navigating
                            // forward.
                            if (isLocationBarShownInNtp()
                                    && mUrlFocusChangeFraction < 1.0f
                                    && mOptionalButtonCoordinator.getButtonView() != null
                                    && mOptionalButtonCoordinator.getButtonView().getVisibility()
                                            != View.GONE) {
                                return mOptionalButtonCoordinator.getButtonView();
                            }
                            // If the url_bar is within the toolbar, the default behavior is to move
                            // to the url_bar when navigating forward.
                            return null;
                        }
                    });

            mOptionalButtonCoordinator
                    .getButtonView()
                    .setOnKeyListener(
                            new KeyboardNavigationListener() {
                                @Override
                                public @Nullable View getNextFocusBackward() {
                                    // If the url_bar is outside the toolbar's bounds and the home
                                    // button is visible, the
                                    // home button is the next button to go in order to bypass the
                                    // url_bar when navigating backward.
                                    if (isLocationBarShownInNtp()
                                            && mUrlFocusChangeFraction < 1.0f
                                            && mHomeButtonDisplay.getVisibility() != View.GONE) {
                                        return mHomeButtonDisplay.getView();
                                    }
                                    // If the url_bar is within the toolbar or the home button is
                                    // not visible in the normal new tab page, the default behavior
                                    // is to move to the url_bar when navigating backward.
                                    return null;
                                }
                            });
        }
    }

    @Override
    @SuppressWarnings("NullAway")
    protected void updateOptionalButton(@Nullable ButtonData buttonData) {
        mButtonData = buttonData;

        if (mOptionalButtonCoordinator == null) {
            initializeOptionalButton();
        }

        mOptionalButtonCoordinator.updateButton(buttonData, isIncognitoBranded());
    }

    @Override
    protected void onMenuButtonDisabled() {
        super.onMenuButtonDisabled();
        // Menu button should always be enabled on ToolbarPhone.
        assert false;
    }

    @Override
    protected void hideOptionalButton() {
        mButtonData = null;
        if (mOptionalButtonCoordinator == null
                || mOptionalButtonCoordinator.getViewVisibility() == View.GONE
                || mLayoutLocationBarWithoutExtraButton) {
            return;
        }

        mOptionalButtonCoordinator.hideButton();
    }

    @Override
    public @Nullable View getOptionalButtonViewForTesting() {
        if (mOptionalButtonCoordinator != null) {
            return mOptionalButtonCoordinator.getButtonView();
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

    private void cancelAnimations() {
        if (mUrlFocusLayoutAnimator != null && mUrlFocusLayoutAnimator.isRunning()) {
            mUrlFocusLayoutAnimator.cancel();
        } else {
            endFocusTransition(urlHasFocus());
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
     *     animation starting and the unfocus animation starting.
     * @return The offset for the location bar when showing the dse icon.
     */
    int getLocationBarOffsetForFocusAnimation(boolean hasFocus) {
        StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
        if (statusCoordinator == null) return 0;

        var profile = getToolbarDataProvider().getProfile();
        if (profile == null
                || !SearchEngineUtils.getForProfile(profile).shouldShowSearchEngineLogo()) {
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
        int offset = statusCoordinator.getStatusIconWidth() - getAdditionalOffsetForNtp();
        return mLocationBar.isLayoutRtl() ? offset : -offset;
    }

    private int getAdditionalOffsetForNtp() {
        return getResources().getDimensionPixelSize(R.dimen.fake_search_box_lateral_padding)
                - getResources().getDimensionPixelSize(R.dimen.location_bar_start_padding);
    }

    @Override
    public boolean isAnimationRunningForTesting() {
        return mUrlFocusChangeInProgress
                || mBrandColorTransitionActive
                || mOptionalButtonAnimationRunning;
    }

    /**
     * @param toolbarColorObserver The observer that observes toolbar color change.
     */
    @Override
    public void setToolbarColorObserver(ToolbarColorObserver toolbarColorObserver) {
        super.setToolbarColorObserver(toolbarColorObserver);
        notifyToolbarColorChanged(mToolbarBackground.getColor());
    }

    @Override
    public void onSuggestionDropdownScroll() {
        mDropdownListScrolled = true;
        updateToolbarAndLocationBarColor();
    }

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {
        mDropdownListScrolled = false;
        updateToolbarAndLocationBarColor();
    }

    private void updateToolbarAndLocationBarColor() {
        @ColorInt
        int toolbarDefaultColor = getToolbarDefaultColor(/* shouldUseFocusColor= */ false);
        updateToolbarBackground(toolbarDefaultColor);
        updateModernLocationBarColor(
                getLocationBarDefaultColorForToolbarColor(
                        toolbarDefaultColor, /* shouldUseFocusColor= */ false));
    }

    private int calculateOnFocusHeightIncrease() {
        return (int) (mBackgroundHeightIncreaseWhenFocus * mUrlFocusChangeFraction / 2);
    }

    @Override
    public void onTransitionStart() {
        setVisibility(View.VISIBLE);
        mInLayoutTransition = true;
        updateToolbarBackgroundFromState(mVisualState);
    }

    @Override
    public void onTransitionEnd() {
        mInLayoutTransition = false;
        updateToolbarBackgroundFromState(mVisualState);
    }

    @Override
    public void requestKeyboardFocus() {
        setFocusOnFirstFocusableDescendant(this);
        // TODO(crbug.com/360423850): Replace this setFocus(mLocationBar) when omnibox keyboard
        // behavior is fixed.
    }

    /**
     * @return The height of the background drawable for the location bar.
     */
    public int getLocationBarBackgroundHeightForTesting() {
        return (mLocationBarBackgroundBounds.bottom + mLocationBarBackgroundNtpOffset.bottom)
                - (mLocationBarBackgroundBounds.top + mLocationBarBackgroundNtpOffset.top);
    }

    void setNtpSearchBoxScrollFractionForTesting(float ntpSearchBoxScrollFraction) {
        mNtpSearchBoxScrollFraction = ntpSearchBoxScrollFraction;
    }

    private void invokeTransition() {
        invokeTransition(/* resetNtpTransition= */ false, /* skipUrlExpansion= */ false);
    }

    /**
     * Triggers NTP transition animation (if toolbar is shown on NTP) and URL expansion animation.
     *
     * @param resetNtpTransition if the transition is to reset NTP animation.
     * @param skipUrlExpansion if the URL expansion animation should be skipped.
     */
    private void invokeTransition(boolean resetNtpTransition, boolean skipUrlExpansion) {
        if (resetNtpTransition) {
            // skipUrlExpansion should not be set when resetNtpTransition is true.
            assert !skipUrlExpansion;
            resetNtpAnimationValues();
            return;
        }

        // No-op when entering or in tab switcher.
        if (inOrEnteringTabSwitcher()) {
            return;
        }

        /*
         * Invoke NTP translation before URL expansion - NTP animation determines offsets for
         * locationBar and urlAction X-translations during URL expansion.
         */
        if (isLocationBarShownInNtp()) {
            updateNtpTransitionAnimation();
        } else {
            resetNtpAnimationValues();
        }

        if (!skipUrlExpansion) {
            updateUrlExpansionAnimation();
        }
    }

    private boolean inOrEnteringTabSwitcher() {
        return mTabSwitcherState == TAB_SWITCHER || mTabSwitcherState == ENTERING_TAB_SWITCHER;
    }
}
