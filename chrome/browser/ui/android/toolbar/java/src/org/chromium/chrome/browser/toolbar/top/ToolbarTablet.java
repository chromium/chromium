// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LevelListDrawable;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.KeyboardNavigationListener;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.NavigationPopup.HistoryDelegate;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.function.BooleanSupplier;

/** The Toolbar object for Tablet screens. */
@SuppressLint("Instantiatable")
public class ToolbarTablet extends ToolbarLayout
        implements OnClickListener, View.OnLongClickListener {
    /** Downloads page for offline access. */
    public interface OfflineDownloader {
        /**
         * Trigger the download of a page.
         *
         * @param context Context to pull resources from.
         * @param tab Tab containing the page to download.
         */
        void downloadPage(Context context, Tab tab);
    }

    private static final int HOME_BUTTON_POSITION_FOR_TAB_STRIP_REDESIGN = 3;

    private ImageButton mHomeButton;
    private ImageButton mBackButton;
    private ImageButton mForwardButton;
    private ImageButton mReloadButton;
    private ImageButton mBookmarkButton;
    private ImageButton mSaveOfflineButton;
    private View mIncognitoIndicator;

    private OnClickListener mBookmarkListener;

    private boolean mIsInTabSwitcherMode;
    private boolean mToolbarButtonsVisible;
    private ImageButton[] mToolbarButtons;
    private ImageButton mOptionalButton;
    private boolean mOptionalButtonUsesTint;

    private NavigationPopup mNavigationPopup;

    private Boolean mIsIncognitoBranded;
    private LocationBarCoordinator mLocationBar;

    private final int mStartPaddingWithButtons;
    private final int mStartPaddingWithoutButtons;
    private boolean mShouldAnimateButtonVisibilityChange;
    private AnimatorSet mButtonVisibilityAnimators;
    private HistoryDelegate mHistoryDelegate;
    private OfflineDownloader mOfflineDownloader;
    private ObservableSupplier<Integer> mTabCountSupplier;
    private TabletCaptureStateToken mLastCaptureStateToken;
    private @DrawableRes int mBookmarkButtonImageRes;
    private OnTouchListener mReloadButtonTouchListener;
    private boolean mIsShiftDownForReload;

    /**
     * Constructs a ToolbarTablet object.
     *
     * @param context The Context in which this View object is created.
     * @param attrs The AttributeSet that was specified with this View.
     */
    public ToolbarTablet(Context context, AttributeSet attrs) {
        super(context, attrs);
        mStartPaddingWithButtons =
                getResources().getDimensionPixelOffset(R.dimen.tablet_toolbar_start_padding);
        mStartPaddingWithoutButtons =
                getResources().getDimensionPixelOffset(R.dimen.toolbar_edge_padding);
    }

    public boolean isToolbarButtonReorderingEnabled() {
        return ChromeFeatureList.sTabletToolbarReordering.isEnabled();
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mHomeButton = findViewById(R.id.home_button);
        mBackButton = findViewById(R.id.back_button);
        mForwardButton = findViewById(R.id.forward_button);
        mReloadButton = findViewById(R.id.refresh_button);

        // Reposition home button to align with desktop ordering when TSR enabled and toolbar
        // reordering not disabled
        if (isToolbarButtonReorderingEnabled()) {
            // Remove home button view added in XML and adding back with different ordering
            // programmatically.
            ((ViewGroup) mHomeButton.getParent()).removeView(mHomeButton);
            LinearLayout linearlayout = (LinearLayout) findViewById(R.id.toolbar_tablet_layout);
            linearlayout.addView(mHomeButton, HOME_BUTTON_POSITION_FOR_TAB_STRIP_REDESIGN);
        }

        // ImageView tinting doesn't work with LevelListDrawable, use Drawable tinting instead.
        // See https://crbug.com/891593 for details.
        // Also, using Drawable tinting doesn't work correctly with LevelListDrawable on Android L
        // and M. As a workaround, we are constructing the LevelListDrawable programmatically. See
        // https://crbug.com/958031 for details.
        final LevelListDrawable reloadIcon = new LevelListDrawable();
        final int reloadLevel = getResources().getInteger(R.integer.reload_button_level_reload);
        final int stopLevel = getResources().getInteger(R.integer.reload_button_level_stop);
        final Drawable reloadLevelDrawable =
                UiUtils.getTintedDrawable(
                        getContext(),
                        R.drawable.btn_toolbar_reload,
                        R.color.default_icon_color_tint_list);
        reloadIcon.addLevel(reloadLevel, reloadLevel, reloadLevelDrawable);
        final Drawable stopLevelDrawable =
                UiUtils.getTintedDrawable(
                        getContext(), R.drawable.btn_close, R.color.default_icon_color_tint_list);
        reloadIcon.addLevel(stopLevel, stopLevel, stopLevelDrawable);
        mReloadButton.setImageDrawable(reloadIcon);

        mBookmarkButton = findViewById(R.id.bookmark_button);
        mSaveOfflineButton = findViewById(R.id.save_offline_button);
        setIncognitoIndicatorVisibility();

        // Initialize values needed for showing/hiding toolbar buttons when the activity size
        // changes.
        mShouldAnimateButtonVisibilityChange = false;
        mToolbarButtonsVisible = true;
        mToolbarButtons = new ImageButton[] {mBackButton, mForwardButton, mReloadButton};

        setTooltipTextForToolbarButtons();
    }

    // Set hover tooltip texts for tablets buttons.
    @Override
    public void setTooltipTextForToolbarButtons() {
        // Set hover tooltip texts for toolbar buttons shared between phones and tablets.
        super.setTooltipTextForToolbarButtons();

        // Set hover tooltip texts for toolbar buttons that only on tablets.
        super.setTooltipText(
                mBackButton, getContext().getString(R.string.accessibility_toolbar_btn_back));
        super.setTooltipText(
                mForwardButton, getContext().getString(R.string.accessibility_menu_forward));
        super.setTooltipText(
                mReloadButton, getContext().getString(R.string.accessibility_btn_refresh));
        super.setTooltipText(
                mBookmarkButton, getContext().getString(R.string.accessibility_menu_bookmark));
        super.setTooltipText(mSaveOfflineButton, getContext().getString(R.string.download_page));
    }

    @Override
    public void setLocationBarCoordinator(LocationBarCoordinator locationBarCoordinator) {
        mLocationBar = locationBarCoordinator;
        final @ColorInt int color =
                ChromeColors.getSurfaceColor(getContext(), R.dimen.default_elevation_2);
        mLocationBar.getTabletCoordinator().tintBackground(color);
    }

    /**
     * Sets up key listeners after native initialization is complete, so that we can invoke native
     * functions.
     */
    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        mHomeButton.setOnClickListener(this);
        mHomeButton.setOnKeyListener(
                new KeyboardNavigationListener() {
                    @Override
                    public View getNextFocusForward() {
                        if (isToolbarButtonReorderingEnabled()) {
                            return findViewById(R.id.url_bar);
                        } else {
                            if (mBackButton.isFocusable()) {
                                return findViewById(R.id.back_button);
                            } else if (mForwardButton.isFocusable()) {
                                return findViewById(R.id.forward_button);
                            } else {
                                return findViewById(R.id.refresh_button);
                            }
                        }
                    }

                    @Override
                    public View getNextFocusBackward() {
                        if (isToolbarButtonReorderingEnabled()) {
                            return findViewById(R.id.refresh_button);
                        } else {
                            return findViewById(R.id.menu_button);
                        }
                    }
                });

        mBackButton.setOnClickListener(this);
        mBackButton.setLongClickable(true);
        mBackButton.setOnKeyListener(
                new KeyboardNavigationListener() {
                    @Override
                    public View getNextFocusForward() {
                        if (mForwardButton.isFocusable()) {
                            return findViewById(R.id.forward_button);
                        } else {
                            return findViewById(R.id.refresh_button);
                        }
                    }

                    @Override
                    public View getNextFocusBackward() {
                        if (isToolbarButtonReorderingEnabled()) {
                            return findViewById(R.id.menu_button);
                        } else {
                            if (mHomeButton.getVisibility() == VISIBLE) {
                                return findViewById(R.id.home_button);
                            } else {
                                return findViewById(R.id.menu_button);
                            }
                        }
                    }
                });

        mForwardButton.setOnClickListener(this);
        mForwardButton.setLongClickable(true);
        mForwardButton.setOnKeyListener(
                new KeyboardNavigationListener() {
                    @Override
                    public View getNextFocusForward() {
                        return findViewById(R.id.refresh_button);
                    }

                    @Override
                    public View getNextFocusBackward() {
                        if (mBackButton.isFocusable()) {
                            return mBackButton;
                        } else if (!isToolbarButtonReorderingEnabled()
                                && mHomeButton.getVisibility() == VISIBLE) {
                            return findViewById(R.id.home_button);
                        } else {
                            return findViewById(R.id.menu_button);
                        }
                    }
                });

        mReloadButton.setOnClickListener(this);
        mReloadButton.setOnLongClickListener(this);
        mReloadButton.setOnKeyListener(
                new KeyboardNavigationListener() {
                    @Override
                    public View getNextFocusForward() {
                        if (isToolbarButtonReorderingEnabled()
                                && mHomeButton.getVisibility() == VISIBLE) {
                            return findViewById(R.id.home_button);
                        } else {
                            return findViewById(R.id.url_bar);
                        }
                    }

                    @Override
                    public View getNextFocusBackward() {
                        if (mForwardButton.isFocusable()) {
                            return mForwardButton;
                        } else if (mBackButton.isFocusable()) {
                            return mBackButton;
                        } else if (!isToolbarButtonReorderingEnabled()
                                && mHomeButton.getVisibility() == VISIBLE) {
                            return findViewById(R.id.home_button);
                        } else {
                            return findViewById(R.id.menu_button);
                        }
                    }
                });
        initReloadButtonTouchListener();

        mBookmarkButton.setOnClickListener(this);
        mBookmarkButton.setOnLongClickListener(this);

        getMenuButtonCoordinator()
                .setOnKeyListener(
                        new KeyboardNavigationListener() {
                            @Override
                            public View getNextFocusForward() {
                                return getCurrentTabView();
                            }

                            @Override
                            public View getNextFocusBackward() {
                                return findViewById(R.id.url_bar);
                            }

                            @Override
                            protected boolean handleEnterKeyPress() {
                                return getMenuButtonCoordinator().onEnterKeyPress();
                            }
                        });

        mSaveOfflineButton.setOnClickListener(this);
        mSaveOfflineButton.setOnLongClickListener(this);
    }

    /**
     * Initializes the reload button's touch listener, which exists to detect shift clicks for
     * reload ignoring cache. Suppress lint ClickableViewAccessibility for the call to
     * setOnTouchListener.
     */
    @SuppressLint("ClickableViewAccessibility")
    private void initReloadButtonTouchListener() {
        mReloadButtonTouchListener =
                new OnTouchListener() {
                    @Override
                    @SuppressLint("ClickableViewAccessibility")
                    public boolean onTouch(View view, MotionEvent event) {
                        // For mouse clicks the framework calls onTouch() before onClick(). Capture
                        // the shift key state to determine reload vs. reload ignoring cache.
                        mIsShiftDownForReload =
                                (event.getMetaState() & KeyEvent.META_SHIFT_ON) != 0;
                        return false;
                    }
                };
        mReloadButton.setOnTouchListener(mReloadButtonTouchListener);
    }

    @Override
    public boolean showContextMenuForChild(View originalView) {
        if (mBackButton == originalView) {
            // Display backwards navigation popup.
            displayNavigationPopup(false, mBackButton);
            return true;
        } else if (mForwardButton == originalView) {
            // Display forwards navigation popup.
            displayNavigationPopup(true, mForwardButton);
            return true;
        }
        return super.showContextMenuForChild(originalView);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        // Ensure the the popup is not shown after resuming activity from background.
        if (hasWindowFocus && mNavigationPopup != null) {
            mNavigationPopup.dismiss();
            mNavigationPopup = null;
        }
        super.onWindowFocusChanged(hasWindowFocus);
    }

    private void displayNavigationPopup(boolean isForward, View anchorView) {
        Tab tab = getToolbarDataProvider().getTab();
        if (tab == null || tab.getWebContents() == null) return;
        mNavigationPopup =
                new NavigationPopup(
                        tab.getProfile(),
                        getContext(),
                        tab.getWebContents().getNavigationController(),
                        isForward
                                ? NavigationPopup.Type.TABLET_FORWARD
                                : NavigationPopup.Type.TABLET_BACK,
                        getToolbarDataProvider()::getTab,
                        mHistoryDelegate);
        mNavigationPopup.show(anchorView);
    }

    @Override
    public void onClick(View v) {
        if (mHomeButton == v) {
            recordHomeModuleClickedIfNTPVisible();
            openHomepage();
        } else if (mBackButton == v) {
            boolean isEnabled = mBackButton.isEnabled();
            boolean success = back();
            assert success && isEnabled
                    : "Back button should not be enabled if page can no longer be navigated back.";
            if (success) RecordUserAction.record("MobileToolbarBack");
        } else if (mForwardButton == v) {
            forward();
            RecordUserAction.record("MobileToolbarForward");
        } else if (mReloadButton == v) {
            stopOrReloadCurrentTab(/* ignoreCache= */ mIsShiftDownForReload);
        } else if (mBookmarkButton == v) {
            if (mBookmarkListener != null) {
                mBookmarkListener.onClick(mBookmarkButton);
                RecordUserAction.record("MobileToolbarToggleBookmark");
            }
        } else if (mSaveOfflineButton == v) {
            mOfflineDownloader.downloadPage(getContext(), getToolbarDataProvider().getTab());
            RecordUserAction.record("MobileToolbarDownloadPage");
        }
    }

    @Override
    public boolean onLongClick(View v) {
        String description = null;
        Context context = getContext();
        Resources resources = context.getResources();

        if (v == mReloadButton) {
            description =
                    (mReloadButton.getDrawable().getLevel()
                                    == resources.getInteger(R.integer.reload_button_level_reload))
                            ? resources.getString(R.string.refresh)
                            : resources.getString(R.string.menu_stop_refresh);
        } else if (v == mBookmarkButton) {
            description = resources.getString(R.string.menu_bookmark);
        } else if (v == mSaveOfflineButton) {
            description = resources.getString(R.string.menu_download);
        }
        return Toast.showAnchoredToast(context, v, description);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        if (textureMode) {
            mLastCaptureStateToken = generateCaptureStateToken();
        }
    }

    @Override
    public CaptureReadinessResult isReadyForTextureCapture() {
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            if (urlHasFocus()) {
                return CaptureReadinessResult.notReady(
                        TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS);
            } else if (mIsInTabSwitcherMode) {
                return CaptureReadinessResult.notReady(
                        TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE);
            } else if (mButtonVisibilityAnimators != null) {
                return CaptureReadinessResult.notReady(
                        TopToolbarBlockCaptureReason.TABLET_BUTTON_ANIMATION_IN_PROGRESS);
            } else {
                return getReadinessStateWithSuppression();
            }
        } else {
            return CaptureReadinessResult.unknown(!urlHasFocus());
        }
    }

    private CaptureReadinessResult getReadinessStateWithSuppression() {
        TabletCaptureStateToken currentToken = generateCaptureStateToken();
        final @ToolbarSnapshotDifference int difference =
                currentToken.getAnyDifference(mLastCaptureStateToken);
        if (difference == ToolbarSnapshotDifference.NONE) {
            return CaptureReadinessResult.notReady(TopToolbarBlockCaptureReason.SNAPSHOT_SAME);
        } else {
            return CaptureReadinessResult.readyWithSnapshotDifference(difference);
        }
    }

    private TabletCaptureStateToken generateCaptureStateToken() {
        UrlBarData urlBarData;
        final @DrawableRes int securityIconResource;

        if (ToolbarFeatures.shouldSuppressCaptures()) {
            urlBarData = mLocationBar.getUrlBarData();
            if (urlBarData == null) urlBarData = getToolbarDataProvider().getUrlBarData();
            StatusCoordinator statusCoordinator = mLocationBar.getStatusCoordinator();
            securityIconResource =
                    statusCoordinator == null
                            ? getToolbarDataProvider().getSecurityIconResource(false)
                            : statusCoordinator.getSecurityIconResource();
        } else {
            urlBarData = getToolbarDataProvider().getUrlBarData();
            securityIconResource = getToolbarDataProvider().getSecurityIconResource(false);
        }

        VisibleUrlText visibleUrlText =
                new VisibleUrlText(
                        urlBarData.displayText, mLocationBar.getOmniboxVisibleTextPrefixHint());
        int tabCount = mTabCountSupplier == null ? 0 : mTabCountSupplier.get();

        return new TabletCaptureStateToken(
                mHomeButton,
                mBackButton,
                mForwardButton,
                mReloadButton,
                securityIconResource,
                visibleUrlText,
                mBookmarkButton,
                mBookmarkButtonImageRes,
                mOptionalButton,
                tabCount,
                getWidth());
    }

    @Override
    void onTabOrModelChanged() {
        super.onTabOrModelChanged();
        final boolean incognitoBranded = isIncognitoBranded();
        if (mIsIncognitoBranded == null || mIsIncognitoBranded != incognitoBranded) {
            // TODO (amaralp): Have progress bar observe theme color and incognito changes directly.
            getProgressBar()
                    .setThemeColor(
                            ChromeColors.getDefaultThemeColor(getContext(), incognitoBranded),
                            incognitoBranded);

            mIsIncognitoBranded = incognitoBranded;
        }
        setIncognitoIndicatorVisibility();

        updateNtp();
    }

    @Override
    public void onTintChanged(
            ColorStateList tint,
            ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        ImageViewCompat.setImageTintList(mHomeButton, activityFocusTint);
        ImageViewCompat.setImageTintList(mBackButton, activityFocusTint);
        ImageViewCompat.setImageTintList(mForwardButton, activityFocusTint);
        // The tint of the |mSaveOfflineButton| should not be affected by an activity focus change.
        ImageViewCompat.setImageTintList(mSaveOfflineButton, tint);
        ImageViewCompat.setImageTintList(mReloadButton, activityFocusTint);
        ImageViewCompat.setImageTintList(
                (ImageView) getTabSwitcherButtonCoordinator().getContainerView(),
                activityFocusTint);

        if (mOptionalButton != null && mOptionalButtonUsesTint) {
            ImageViewCompat.setImageTintList(mOptionalButton, activityFocusTint);
        }
    }

    @Override
    public void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {
        setBackgroundColor(color);
        final @ColorInt int textBoxColor =
                ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                        getContext(), color, isIncognitoBranded(), /* isCustomTab= */ false);
        mLocationBar.getTabletCoordinator().tintBackground(textBoxColor);
        mLocationBar.updateVisualsForState();
        setToolbarHairlineColor(color);

        // Notify the StatusBarColorController of the toolbar color change. This is to match the
        // status bar's color with the toolbar color when the tab strip is hidden on a tablet.
        notifyToolbarColorChanged(color);
    }

    /** Called when the currently visible New Tab Page changes. */
    private void updateNtp() {
        NewTabPageDelegate ntpDelegate = getToolbarDataProvider().getNewTabPageDelegate();
        ntpDelegate.setSearchBoxScrollListener(
                (scrollFraction) -> {
                    // Fade the search box out in the first 40% of the scrolling transition.
                    float alpha = Math.max(1f - scrollFraction * 2.5f, 0f);
                    ntpDelegate.setSearchBoxAlpha(alpha);
                    ntpDelegate.setSearchProviderLogoAlpha(alpha);
                });
    }

    @Override
    void onTabContentViewChanged() {
        super.onTabContentViewChanged();
        updateNtp();
    }

    @Override
    void updateButtonVisibility() {
        mLocationBar.updateButtonVisibility();
    }

    @Override
    void updateBackButtonVisibility(boolean canGoBack) {
        boolean enableButton = canGoBack && !mIsInTabSwitcherMode;
        mBackButton.setEnabled(enableButton);
        mBackButton.setFocusable(enableButton);
    }

    @Override
    void updateForwardButtonVisibility(boolean canGoForward) {
        boolean enableButton = canGoForward && !mIsInTabSwitcherMode;
        mForwardButton.setEnabled(enableButton);
        mForwardButton.setFocusable(enableButton);
    }

    @Override
    void updateReloadButtonVisibility(boolean isReloading) {
        if (isReloading) {
            mReloadButton
                    .getDrawable()
                    .setLevel(getResources().getInteger(R.integer.reload_button_level_stop));
            mReloadButton.setContentDescription(
                    getContext().getString(R.string.accessibility_btn_stop_loading));
        } else {
            mReloadButton
                    .getDrawable()
                    .setLevel(getResources().getInteger(R.integer.reload_button_level_reload));
            mReloadButton.setContentDescription(
                    getContext().getString(R.string.accessibility_btn_refresh));
        }
        mReloadButton.setEnabled(!mIsInTabSwitcherMode);
    }

    @Override
    void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) {
        if (isBookmarked) {
            mBookmarkButtonImageRes = R.drawable.btn_star_filled;
            mBookmarkButton.setImageResource(R.drawable.btn_star_filled);
            final @ColorRes int tint =
                    isIncognitoBranded()
                            ? R.color.default_icon_color_blue_light
                            : R.color.default_icon_color_accent1_tint_list;
            ImageViewCompat.setImageTintList(
                    mBookmarkButton, AppCompatResources.getColorStateList(getContext(), tint));
            mBookmarkButton.setContentDescription(getContext().getString(R.string.edit_bookmark));
        } else {
            mBookmarkButtonImageRes = R.drawable.star_outline_24dp;
            mBookmarkButton.setImageResource(R.drawable.star_outline_24dp);
            ImageViewCompat.setImageTintList(mBookmarkButton, getTint());
            mBookmarkButton.setContentDescription(
                    getContext().getString(R.string.accessibility_menu_bookmark));
        }
        mBookmarkButton.setEnabled(editingAllowed);
    }

    @Override
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        mIsInTabSwitcherMode = inTabSwitcherMode;
        int importantForAccessibility =
                inTabSwitcherMode
                        ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;

        mLocationBar.setUrlBarFocusable(!mIsInTabSwitcherMode);
        if (getImportantForAccessibility() != importantForAccessibility) {
            setImportantForAccessibility(importantForAccessibility);
            sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }

    @Override
    public void initialize(
            ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController,
            MenuButtonCoordinator menuButtonCoordinator,
            ToggleTabStackButtonCoordinator tabSwitcherButtonCoordinator,
            HistoryDelegate historyDelegate,
            BooleanSupplier partnerHomepageEnabledSupplier,
            OfflineDownloader offlineDownloader,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Tracker> trackerSupplier) {
        super.initialize(
                toolbarDataProvider,
                tabController,
                menuButtonCoordinator,
                tabSwitcherButtonCoordinator,
                historyDelegate,
                partnerHomepageEnabledSupplier,
                offlineDownloader,
                userEducationHelper,
                trackerSupplier);
        mHistoryDelegate = historyDelegate;
        mOfflineDownloader = offlineDownloader;
        menuButtonCoordinator.setVisibility(true);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mButtonVisibilityAnimators != null) {
            mButtonVisibilityAnimators.removeAllListeners();
            mButtonVisibilityAnimators.cancel();
            mButtonVisibilityAnimators = null;
        }
    }

    @Override
    void setTabCountSupplier(ObservableSupplier<Integer> tabCountSupplier) {
        mTabCountSupplier = tabCountSupplier;
    }

    @Override
    void setBookmarkClickHandler(OnClickListener listener) {
        mBookmarkListener = listener;
    }

    @Override
    void onHomeButtonUpdate(boolean homeButtonEnabled) {
        mHomeButton.setVisibility(homeButtonEnabled ? VISIBLE : GONE);
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // After the first layout, button visibility changes should be animated. On the first
        // layout, the button visibility shouldn't be animated because the visibility may be
        // changing solely because Chrome was launched into multi-window.
        mShouldAnimateButtonVisibilityChange = true;

        super.onLayout(changed, left, top, right, bottom);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Hide or show toolbar buttons if needed. With the introduction of multi-window on
        // Android N, the Activity can be < 600dp, in which case the toolbar buttons need to be
        // moved into the menu so that the location bar is usable. The buttons must be shown
        // in onMeasure() so that the location bar gets measured and laid out correctly.
        setToolbarButtonsVisible(
                MeasureSpec.getSize(widthMeasureSpec)
                        >= DeviceFormFactor.getNonMultiDisplayMinimumTabletWidthPx(getContext()));

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    void updateOptionalButton(ButtonData buttonData) {
        if (mOptionalButton == null) {
            ViewStub viewStub = findViewById(R.id.optional_button_stub);
            mOptionalButton = (ImageButton) viewStub.inflate();
        }

        ButtonSpec buttonSpec = buttonData.getButtonSpec();

        // Set hover highlight for profile, voice search, share and new tab button on tablets. Set
        // box hover highlight for the rest of button variants.
        if (buttonData.getButtonSpec().getShouldShowHoverHighlight()) {
            mOptionalButton.setBackgroundResource(R.drawable.toolbar_button_ripple);
        } else {
            TypedValue themeRes = new TypedValue();
            getContext()
                    .getTheme()
                    .resolveAttribute(R.attr.selectableItemBackground, themeRes, true);
            mOptionalButton.setBackgroundResource(themeRes.resourceId);
        }

        // Set hover tooltip text for voice search, share and new tab button on tablets.
        if (buttonSpec.getHoverTooltipTextId() != ButtonSpec.INVALID_TOOLTIP_TEXT_ID) {
            super.setTooltipText(
                    mOptionalButton, getContext().getString(buttonSpec.getHoverTooltipTextId()));
        } else {
            super.setTooltipText(mOptionalButton, null);
        }

        mOptionalButtonUsesTint = buttonSpec.getSupportsTinting();
        if (mOptionalButtonUsesTint) {
            ImageViewCompat.setImageTintList(mOptionalButton, getTint());
        } else {
            ImageViewCompat.setImageTintList(mOptionalButton, null);
        }

        if (buttonSpec.getIPHCommandBuilder() != null) {
            buttonSpec.getIPHCommandBuilder().setAnchorView(mOptionalButton);
        }
        mOptionalButton.setOnClickListener(buttonSpec.getOnClickListener());
        if (buttonSpec.getOnLongClickListener() == null) {
            mOptionalButton.setLongClickable(false);
        } else {
            mOptionalButton.setLongClickable(true);
            mOptionalButton.setOnLongClickListener(buttonSpec.getOnLongClickListener());
        }
        mOptionalButton.setImageDrawable(buttonSpec.getDrawable());
        mOptionalButton.setContentDescription(buttonSpec.getContentDescription());
        mOptionalButton.setVisibility(View.VISIBLE);
        mOptionalButton.setEnabled(buttonData.isEnabled());
    }

    @Override
    void hideOptionalButton() {
        if (mOptionalButton == null || mOptionalButton.getVisibility() == View.GONE) {
            return;
        }

        mOptionalButton.setVisibility(View.GONE);
    }

    @Override
    public View getOptionalButtonViewForTesting() {
        return mOptionalButton;
    }

    @Override
    public ImageView getHomeButton() {
        return mHomeButton;
    }

    private void setIncognitoIndicatorVisibility() {
        if (mIsIncognitoBranded == null
                || !ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()) return;
        if (mIncognitoIndicator == null && mIsIncognitoBranded) {
            ViewStub stub = findViewById(R.id.incognito_indicator_stub);
            mIncognitoIndicator = stub.inflate();
        }
        if (mIncognitoIndicator != null) {
            mIncognitoIndicator.setVisibility(
                    mIsIncognitoBranded && mToolbarButtonsVisible ? VISIBLE : GONE);
        }
    }

    private void setToolbarButtonsVisible(boolean visible) {
        if (mToolbarButtonsVisible == visible) return;

        mToolbarButtonsVisible = visible;

        if (mShouldAnimateButtonVisibilityChange) {
            runToolbarButtonsVisibilityAnimation(visible);
        } else {
            for (ImageButton button : mToolbarButtons) {
                button.setVisibility(visible ? View.VISIBLE : View.GONE);
            }
            mLocationBar.setShouldShowButtonsWhenUnfocusedForTablet(visible);
            setStartPaddingBasedOnButtonVisibility(visible);
            setIncognitoIndicatorVisibility();
        }
    }

    /**
     * Sets the toolbar start padding based on whether the buttons are visible.
     *
     * @param buttonsVisible Whether the toolbar buttons are visible.
     */
    private void setStartPaddingBasedOnButtonVisibility(boolean buttonsVisible) {
        buttonsVisible = buttonsVisible || mHomeButton.getVisibility() == View.VISIBLE;

        this.setPaddingRelative(
                buttonsVisible ? mStartPaddingWithButtons : mStartPaddingWithoutButtons,
                getPaddingTop(),
                ViewCompat.getPaddingEnd(this),
                getPaddingBottom());
    }

    /**
     * @return The difference in start padding when the buttons are visible and when they are not
     * visible.
     */
    public int getStartPaddingDifferenceForButtonVisibilityAnimation() {
        // If the home button is visible then the padding doesn't change.
        return mHomeButton.getVisibility() == View.VISIBLE
                ? 0
                : mStartPaddingWithButtons - mStartPaddingWithoutButtons;
    }

    private void runToolbarButtonsVisibilityAnimation(boolean visible) {
        if (mButtonVisibilityAnimators != null) mButtonVisibilityAnimators.cancel();

        mButtonVisibilityAnimators =
                visible ? buildShowToolbarButtonsAnimation() : buildHideToolbarButtonsAnimation();
        mButtonVisibilityAnimators.start();
    }

    private AnimatorSet buildShowToolbarButtonsAnimation() {
        Collection<Animator> animators = new ArrayList<>();

        // Create animators for all of the toolbar buttons.
        for (ImageButton button : mToolbarButtons) {
            animators.add(mLocationBar.createShowButtonAnimatorForTablet(button));
        }

        // Add animators for location bar.
        animators.addAll(
                mLocationBar.getShowButtonsWhenUnfocusedAnimatorsForTablet(
                        getStartPaddingDifferenceForButtonVisibilityAnimation()));

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animators);

        set.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        keepControlsShownForAnimation();
                        for (ImageButton button : mToolbarButtons) {
                            button.setVisibility(View.VISIBLE);
                        }
                        // Set the padding at the start of the animation so the toolbar buttons
                        // don't jump when the animation ends.
                        setStartPaddingBasedOnButtonVisibility(true);
                        setIncognitoIndicatorVisibility();
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mButtonVisibilityAnimators = null;
                        allowBrowserControlsHide();
                    }
                });

        return set;
    }

    private AnimatorSet buildHideToolbarButtonsAnimation() {
        Collection<Animator> animators = new ArrayList<>();

        // Create animators for all of the toolbar buttons.
        for (ImageButton button : mToolbarButtons) {
            ObjectAnimator hideButtonAnimator =
                    mLocationBar.createHideButtonAnimatorForTablet(button);
            if (hideButtonAnimator != null) {
                animators.add(hideButtonAnimator);
            }
        }

        // Add animators for location bar.
        animators.addAll(
                mLocationBar.getHideButtonsWhenUnfocusedAnimatorsForTablet(
                        getStartPaddingDifferenceForButtonVisibilityAnimation()));

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animators);

        set.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        keepControlsShownForAnimation();

                        setIncognitoIndicatorVisibility();
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Only set end visibility and alpha if the animation is ending because it's
                        // completely finished and not because it was canceled.
                        if (mToolbarButtons[0].getAlpha() == 0.f) {
                            for (ImageButton button : mToolbarButtons) {
                                button.setVisibility(View.GONE);
                                button.setAlpha(1.f);
                            }
                            // Set the padding at the end of the animation so the toolbar buttons
                            // don't jump when the animation starts.
                            setStartPaddingBasedOnButtonVisibility(false);
                        }
                        mButtonVisibilityAnimators = null;
                        allowBrowserControlsHide();
                    }
                });

        return set;
    }

    @VisibleForTesting
    ImageButton[] getToolbarButtons() {
        return mToolbarButtons;
    }

    void enableButtonVisibilityChangeAnimationForTesting() {
        mShouldAnimateButtonVisibilityChange = true;
    }

    void setToolbarButtonsVisibleForTesting(boolean value) {
        mToolbarButtonsVisible = value;
    }

    public ImageButton getBookmarkButtonForTesting() {
        return mBookmarkButton;
    }

    OnTouchListener getReloadButtonTouchListenerForTest() {
        return mReloadButtonTouchListener;
    }
}
