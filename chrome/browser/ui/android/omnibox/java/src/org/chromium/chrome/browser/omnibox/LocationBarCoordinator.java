// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.ActionMode;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.WindowInsets;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.OmniboxUma;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator.PageInfoAction;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarVariationUtils;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.OptionalButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.accessibility.PageZoomIndicatorCoordinator;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * The public API of the location bar component. Location bar responsibilities are:
 *
 * <ul>
 *   <li>Display the current URL.
 *   <li>Display Status.
 *   <li>Handle omnibox input.
 * </ul>
 *
 * <p>The coordinator creates and owns elements within this component.
 */
@NullMarked
public class LocationBarCoordinator
        implements LocationBar, NativeInitObserver, AutocompleteDelegate {

    private static final int COMPACT_MODE_FADE_START_DELAY_MS = 100;
    private static final int COMPACT_MODE_ANIMATION_DURATION_MS = 150;
    private final DeferredIMEWindowInsetApplicationCallback
            mDeferredIMEWindowInsetApplicationCallback;

    private OmniboxSuggestionsDropdownEmbedderImpl mOmniboxDropdownEmbedderImpl;
    private int mCurrentFuseboxState = FuseboxState.DISABLED;

    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator {
        /** Destroys SubCoordinator. */
        void destroy();
    }

    private LocationBarLayout mLocationBarLayout;
    private @Nullable SubCoordinator mSubCoordinator;
    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private LocationBarEmbedder mLocationBarEmbedder;
    private final @Nullable BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final boolean mIsToolbarPositionCustomizationEnabled;
    private final View mBottomContainerView;
    private UrlBarCoordinator mUrlCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private StatusCoordinator mStatusCoordinator;
    private FuseboxCoordinator mFuseboxCoordinator;
    private final WindowAndroid mWindowAndroid;
    private final Callback<Boolean> mTextWrappingListener;
    private final Callback<@FuseboxState Integer> mOnFuseboxStateChange =
            this::onFuseboxStateChange;
    private final SettableMonotonicObservableSupplier<Tracker> mTrackerSupplier =
            ObservableSuppliers.createMonotonic();
    private final @Nullable UserEducationHelper mUserEducationHelper;
    private LocationBarMediator mLocationBarMediator;
    private View mUrlBar;
    private View mZoomButton;
    private @Nullable View mDeleteButton;
    private @Nullable View mBackButton;
    private @Nullable View mNavigateButton;
    private @Nullable View mMicButton;
    private @Nullable View mLensButton;
    private @Nullable View mBookmarksButton;
    private @Nullable View mInstallButton;
    private @Nullable PageZoomIndicatorCoordinator mPageZoomIndicatorCoordinator;
    private CallbackController mCallbackController = new CallbackController();
    private boolean mDestroyed;
    private @Nullable OptionalButtonCoordinator mOptionalButtonCoordinator;
    private @Nullable ButtonData mOptionalButtonData;
    private LocationBarDataProvider.@Nullable Observer mOptionalButtonLocationBarDataObserver;
    private @Nullable UrlFocusChangeListener mOptionalButtonUrlFocusChangeListener;

    private boolean mNativeInitialized;

    /**
     * Creates {@link LocationBarCoordinator} and its subcoordinator: {@link
     * LocationBarCoordinatorPhone} or {@link LocationBarCoordinatorTablet}, depending on the type
     * of {@code locationBarLayout}; no sub-coordinator is created for other LocationBarLayout
     * subclasses. {@code LocationBarCoordinator} owns the subcoordinator. Destroying the former
     * destroys the latter.
     *
     * @param locationBarLayout Inflated {@link LocationBarLayout}. {@code LocationBarCoordinator}
     *     takes ownership and will destroy this object.
     * @param autocompleteAnchorView The view to anchor the autocomplete dropdown to.
     * @param profileObservableSupplier The supplier of the active profile.
     * @param locationBarDataProvider {@link LocationBarDataProvider} to be used for accessing
     *     Toolbar state.
     * @param actionModeCallback The default callback for text editing action bar to use.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param activityTabSupplier A Supplier to access the activity's current tab.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} object.
     * @param shareDelegateSupplier A supplier for {@link ShareDelegate} object.
     * @param incognitoStateProvider An {@link IncognitoStateProvider} to access the current
     *     incognito state.
     * @param activityLifecycleDispatcher Allows observation of the activity state.
     * @param overrideUrlLoadingDelegate Delegate that allows customization of url loading behavior.
     * @param backKeyBehavior Delegate that allows customization of back key behavior.
     * @param pageInfoAction Displays page info popup.
     * @param bringTabGroupToFrontCallback Callback to bring the browser foreground and switch to a
     *     tab group.
     * @param omniboxUma Interface for logging UMA histogram.
     * @param bookmarkState State of a URL bookmark state.
     * @param isToolbarMicEnabledSupplier Whether toolbar mic is enabled or not.
     * @param omniboxActionDelegate Delegate for handling omnibox actions.
     * @param browserControlsVisibilityDelegate Delegate for browser controls visibility.
     * @param backPressManager The {@link BackPressManager} for intercepting back press.
     * @param omniboxSuggestionsDropdownScrollListener Listener for suggestions dropdown scroll.
     * @param tabModelSelectorSupplier Supplier of the {@link TabModelSelector}.
     * @param topInsetProvider Provider for top insets.
     * @param locationBarEmbedder Embedder for location bar.
     * @param uiOverrides embedder-specific UI overrides
     * @param baseChromeLayout The base view hosting Chrome that certain views (e.g. the omnibox
     *     suggestion list) will position themselves relative to. If null, the content view will be
     *     used.
     * @param bottomWindowPaddingSupplier Supplier of the height of the bottom-most region of the
     *     window that should be considered part of the window's height. This region is suitable for
     *     rendering content, particularly to achieve a full-bleed visual effect, though it should
     *     also be incorporated as bottom padding to ensure that such content can be fully scrolled
     *     out of this region to be fully visible and interactable. This is used to ensure the
     *     suggestions list draws edge to edge when appropriate. This should only be used when the
     *     soft keyboard is not visible.
     * @param onLongClickListener for the url bar.
     * @param browserControlsStateProvider Provider for browser controls state.
     * @param isToolbarPositionCustomizationEnabled Whether toolbar position customization is
     *     enabled.
     * @param pageZoomManager The {@link PageZoomManager} for managing the page zoom.
     * @param tabFaviconFunction Function to get tab favicon.
     * @param snackbarManager Manager for snackbars.
     * @param scrimManager Manager for scrims.
     * @param bottomContainerView The bottom container view.
     * @param omniboxChipManager The {@link OmniboxChipManager} to show chips in the omnibox.
     * @param userEducationHelper The {@link UserEducationHelper} to show any user education events.
     */
    public LocationBarCoordinator(
            View locationBarLayout,
            View autocompleteAnchorView,
            MonotonicObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ActionMode.@Nullable Callback actionModeCallback,
            WindowAndroid windowAndroid,
            NullableObservableSupplier<Tab> activityTabSupplier,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            @Nullable Supplier<ShareDelegate> shareDelegateSupplier,
            @Nullable IncognitoStateProvider incognitoStateProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            BackKeyBehaviorDelegate backKeyBehavior,
            PageInfoAction pageInfoAction,
            Callback<String> bringTabGroupToFrontCallback,
            OmniboxUma omniboxUma,
            BookmarkState bookmarkState,
            BooleanSupplier isToolbarMicEnabledSupplier,
            OmniboxActionDelegateImpl omniboxActionDelegate,
            @Nullable BrowserStateBrowserControlsVisibilityDelegate
                    browserControlsVisibilityDelegate,
            @Nullable BackPressManager backPressManager,
            @Nullable OmniboxSuggestionsDropdownScrollListener
                    omniboxSuggestionsDropdownScrollListener,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            TopInsetProvider topInsetProvider,
            LocationBarEmbedder locationBarEmbedder,
            LocationBarEmbedderUiOverrides uiOverrides,
            @Nullable View baseChromeLayout,
            Supplier<Integer> bottomWindowPaddingSupplier,
            @Nullable OnLongClickListener onLongClickListener,
            @Nullable BrowserControlsStateProvider browserControlsStateProvider,
            boolean isToolbarPositionCustomizationEnabled,
            @Nullable PageZoomManager pageZoomManager,
            Function<Tab, @Nullable Bitmap> tabFaviconFunction,
            SnackbarManager snackbarManager,
            View bottomContainerView,
            @Nullable OmniboxChipManager omniboxChipManager,
            @Nullable LocationBarFocusScrimHandler scrimHandler,
            @Nullable UserEducationHelper userEducationHelper) {
        mLocationBarLayout = (LocationBarLayout) locationBarLayout;
        mWindowAndroid = windowAndroid;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mLocationBarEmbedder = locationBarEmbedder;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mIsToolbarPositionCustomizationEnabled = isToolbarPositionCustomizationEnabled;
        mBottomContainerView = bottomContainerView;
        mUserEducationHelper = userEducationHelper;
        mActivityLifecycleDispatcher.register(this);
        Context context = mLocationBarLayout.getContext();
        OneshotSupplierImpl<TemplateUrlService> templateUrlServiceSupplier =
                new OneshotSupplierImpl<>();
        mDeferredIMEWindowInsetApplicationCallback =
                new DeferredIMEWindowInsetApplicationCallback(
                        mCallbackController.makeCancelable(
                                () -> {
                                    mOmniboxDropdownEmbedderImpl.recalculateOmniboxAlignment();
                                    updateBottomContainerPosition();
                                }));

        mUrlBar = mLocationBarLayout.findViewById(R.id.url_bar);
        final boolean isIncognito =
                incognitoStateProvider != null && incognitoStateProvider.isIncognitoSelected();
        OmniboxResourceProvider.setTabFaviconFactory(tabFaviconFunction);
        mFuseboxCoordinator =
                new FuseboxCoordinator(
                        context,
                        windowAndroid,
                        mLocationBarLayout,
                        tabModelSelectorSupplier,
                        templateUrlServiceSupplier,
                        snackbarManager,
                        () ->
                                mAutocompleteCoordinator != null
                                        ? mAutocompleteCoordinator.getSuggestionsDropdown()
                                        : null);
        NonNullObservableSupplier<Integer> fuseboxStateSupplier;
        if (OmniboxFeatures.isMultimodalInputEnabled(context)) {
            fuseboxStateSupplier = mFuseboxCoordinator.getFuseboxStateSupplier();
            fuseboxStateSupplier.addSyncObserverAndPostIfNonNull(mOnFuseboxStateChange);
        } else {
            fuseboxStateSupplier = ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
        }

        if (mLocationBarLayout instanceof LocationBarTablet tabletLayout) {
            tabletLayout.setHolder((ViewGroup) tabletLayout.getParent());
        }

        View alignmentView = mLocationBarLayout.getAlignmentView();
        mOmniboxDropdownEmbedderImpl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mWindowAndroid,
                        autocompleteAnchorView,
                        alignmentView,
                        uiOverrides.isForcedPhoneStyleOmnibox(),
                        baseChromeLayout,
                        () ->
                                mBrowserControlsStateProvider == null
                                        ? ControlsPosition.TOP
                                        : mBrowserControlsStateProvider.getControlsPosition(),
                        mDeferredIMEWindowInsetApplicationCallback::getCurrentKeyboardHeight,
                        bottomWindowPaddingSupplier,
                        fuseboxStateSupplier,
                        locationBarDataProvider,
                        topInsetProvider);

        mPageZoomIndicatorCoordinator =
                pageZoomManager != null
                        ? new PageZoomIndicatorCoordinator(() -> mZoomButton, pageZoomManager)
                        : null;

        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.setOnZoomLevelChangedCallback(this::onZoomLevelChanged);
        }
        // TODO(crbug.com/40733049): Inject LocaleManager instance to LocationBarCoordinator instead
        // of using the singleton.
        mLocationBarMediator =
                new LocationBarMediator(
                        context,
                        mLocationBarLayout,
                        locationBarDataProvider,
                        uiOverrides,
                        profileObservableSupplier,
                        overrideUrlLoadingDelegate,
                        LocaleManager.getInstance(),
                        templateUrlServiceSupplier,
                        backKeyBehavior,
                        windowAndroid,
                        isTabletWindow() && isTabletLayout(),
                        LensController.getInstance(),
                        omniboxUma,
                        isToolbarMicEnabledSupplier,
                        mOmniboxDropdownEmbedderImpl,
                        tabModelSelectorSupplier,
                        browserControlsStateProvider,
                        modalDialogManagerSupplier,
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        locationBarEmbedder,
                        omniboxChipManager,
                        scrimHandler);
        mBackButton = mLocationBarLayout.findViewById(R.id.omnibox_back_button);
        if (mBackButton != null) {
            mBackButton.setOnClickListener(v -> mLocationBarMediator.onBackButtonClicked());
        }
        if (backPressManager != null) {
            backPressManager.addHandler(mLocationBarMediator, BackPressHandler.Type.LOCATION_BAR);
        }
        mActivityLifecycleDispatcher.register(mLocationBarMediator);
        mUrlCoordinator =
                new UrlBarCoordinator(
                        context,
                        (UrlBar) mUrlBar,
                        actionModeCallback,
                        mCallbackController.makeCancelable(mLocationBarMediator::onUrlFocusChange),
                        mLocationBarMediator,
                        windowAndroid.getKeyboardDelegate(),
                        isIncognito,
                        onLongClickListener,
                        mLocationBarMediator::onUrlTextChanged,
                        mLocationBarMediator::onUrlTextRichChanged,
                        mLocationBarMediator);

        initializeBoundsEllipsis(locationBarDataProvider);

        // Set up text wrapping listener for FuseboxCoordinator
        mTextWrappingListener = this::onTextWrappingChanged;
        mUrlCoordinator.addTextWrappingChangeListener(mTextWrappingListener);

        mAutocompleteCoordinator =
                new AutocompleteCoordinator(
                        mLocationBarLayout,
                        this,
                        mOmniboxDropdownEmbedderImpl,
                        mUrlCoordinator,
                        modalDialogManagerSupplier,
                        activityTabSupplier,
                        shareDelegateSupplier,
                        locationBarDataProvider,
                        profileObservableSupplier,
                        bringTabGroupToFrontCallback,
                        bookmarkState,
                        omniboxActionDelegate,
                        omniboxSuggestionsDropdownScrollListener,
                        mActivityLifecycleDispatcher,
                        uiOverrides.isForcedPhoneStyleOmnibox(),
                        windowAndroid,
                        mDeferredIMEWindowInsetApplicationCallback,
                        mFuseboxCoordinator);
        StatusView statusView = mLocationBarLayout.findViewById(R.id.location_bar_status);
        mStatusCoordinator =
                new StatusCoordinator(
                        isTabletWindow(),
                        statusView,
                        locationBarDataProvider,
                        templateUrlServiceSupplier,
                        profileObservableSupplier,
                        windowAndroid,
                        pageInfoAction,
                        browserControlsVisibilityDelegate,
                        fuseboxStateSupplier,
                        mFuseboxCoordinator::plusButtonClicked,
                        mLocationBarMediator.getExactMatchUrlSupplier());
        mLocationBarMediator.setCoordinators(
                mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);

        mLocationBarMediator.addUrlFocusChangeListener(
                (focused) -> updateBottomContainerPosition());

        mLocationBarMediator.addUrlFocusChangeListener(mUrlCoordinator);

        mDeleteButton = mLocationBarLayout.findViewById(R.id.delete_button);
        mDeleteButton.setOnClickListener(mLocationBarMediator::deleteButtonClicked);

        mNavigateButton = mLocationBarLayout.findViewById(R.id.navigate_button);
        if (mNavigateButton != null) {
            mNavigateButton.setOnClickListener(mLocationBarMediator::navigateButtonClicked);
        }

        mMicButton = mLocationBarLayout.findViewById(R.id.mic_button);
        mMicButton.setOnClickListener(mLocationBarMediator::micButtonClicked);

        mLensButton = mLocationBarLayout.findViewById(R.id.lens_camera_button);
        mLensButton.setOnClickListener(mLocationBarMediator::lensButtonClicked);

        mZoomButton = mLocationBarLayout.findViewById(R.id.zoom_button);
        mZoomButton.setOnClickListener(mLocationBarMediator::zoomButtonClicked);

        mInstallButton = mLocationBarLayout.findViewById(R.id.install_button);
        mInstallButton.setOnClickListener(mLocationBarMediator::installButtonClicked);

        // The LocationBar's direction is tied to the UrlBar's text direction. Icons inside the
        // location bar, e.g. lock, refresh, X, should be reversed if UrlBar's text is RTL.
        mUrlCoordinator.setUrlDirectionListener(
                mCallbackController.makeCancelable(
                        layoutDirection -> {
                            ViewCompat.setLayoutDirection(mLocationBarLayout, layoutDirection);
                            mAutocompleteCoordinator.updateSuggestionListLayoutDirection();
                        }));

        context.registerComponentCallbacks(mLocationBarMediator);
        mLocationBarLayout.initialize(
                mAutocompleteCoordinator,
                mUrlCoordinator,
                mStatusCoordinator,
                locationBarDataProvider,
                mWindowAndroid);

        Callback<Profile> profileObserver =
                new Callback<>() {
                    @Override
                    public void onResult(Profile profile) {
                        templateUrlServiceSupplier.set(
                                TemplateUrlServiceFactory.getForProfile(profile));
                        mTrackerSupplier.set(TrackerFactory.getTrackerForProfile(profile));
                        profileObservableSupplier.removeObserver(this);
                    }
                };
        profileObservableSupplier.addSyncObserverAndPostIfNonNull(profileObserver);

        if (isPhoneLayout()) {
            mSubCoordinator = new LocationBarCoordinatorPhone((LocationBarPhone) locationBarLayout);
        } else if (isTabletLayout()) {
            mSubCoordinator =
                    new LocationBarCoordinatorTablet((LocationBarTablet) locationBarLayout);
        }
        // There is a third possibility: SearchActivityLocationBarLayout extends LocationBarLayout
        // and can be instantiated on phones *or* tablets.
    }

    @VisibleForTesting
    void initializeBoundsEllipsis(LocationBarDataProvider dataProvider) {
        // TODO(crbug.com/507471408): Revisit logic to guard it more strictly.
        int pageClassification = dataProvider.getPageClassification(/* prefetch= */ false);
        boolean enableBoundsEllipsis =
                pageClassification != PageClassification.ANDROID_HUB_VALUE
                        && pageClassification != PageClassification.OTHER_ON_CCT_VALUE
                        && pageClassification != PageClassification.CO_BROWSING_COMPOSEBOX_VALUE;
        mUrlCoordinator.setBoundsEllipsisEnabled(enableBoundsEllipsis);
    }

    private void updateBottomContainerPosition() {
        var layoutParams = (MarginLayoutParams) mBottomContainerView.getLayoutParams();
        if (mLocationBarMediator.isUrlBarFocused()) {
            View rootView = mLocationBarLayout.getRootView();
            WindowInsets windowInsets = rootView.getRootWindowInsets();
            layoutParams.bottomMargin =
                    windowInsets == null
                            ? 0
                            : WindowInsetsCompat.toWindowInsetsCompat(windowInsets, rootView)
                                    .getInsets(WindowInsetsCompat.Type.ime())
                                    .bottom;
        } else {
            layoutParams.bottomMargin = 0;
        }
    }

    @Override
    public float getUrlBarHeight() {
        return mUrlBar.getHeight();
    }

    /**
     * Sets the callback to be executed when the status view is hidden due to the Page Info removal.
     */
    public void setOnStatusViewHiddenForPageInfoRemoval(Runnable runnable) {
        mStatusCoordinator.setOnStatusViewHiddenForPageInfoRemoval(runnable);
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mSubCoordinator != null) {
            mSubCoordinator.destroy();
            mSubCoordinator = null;
        }

        mUrlBar.setOnKeyListener(null);
        mUrlBar = null;

        mDeleteButton.setOnClickListener(null);
        mDeleteButton = null;

        if (mBackButton != null) {
            mBackButton.setOnClickListener(null);
            mBackButton = null;
        }

        mMicButton.setOnClickListener(null);
        mMicButton = null;

        mLensButton.setOnClickListener(null);
        mLensButton = null;

        if (mNavigateButton != null) {
            mNavigateButton.setOnClickListener(null);
            mNavigateButton = null;
        }

        mZoomButton.setOnClickListener(null);
        mZoomButton = null;

        mInstallButton.setOnClickListener(null);
        mInstallButton = null;

        if (mBookmarksButton != null) {
            mBookmarksButton.setOnClickListener(null);
            mBookmarksButton = null;
        }

        if (mOptionalButtonLocationBarDataObserver != null) {
            mLocationBarMediator
                    .getLocationBarDataProvider()
                    .removeObserver(mOptionalButtonLocationBarDataObserver);
            mOptionalButtonLocationBarDataObserver = null;
        }

        if (mOptionalButtonUrlFocusChangeListener != null) {
            mLocationBarMediator.removeUrlFocusChangeListener(
                    mOptionalButtonUrlFocusChangeListener);
            mOptionalButtonUrlFocusChangeListener = null;
        }

        if (mOptionalButtonCoordinator != null) {
            mLocationBarMediator.setOptionalButtonColorChangeCallback(null);
            mOptionalButtonCoordinator.hideButton();
            mOptionalButtonData = null;
        }

        mLocationBarMediator.removeUrlFocusChangeListener(mUrlCoordinator);
        mUrlCoordinator.removeTextWrappingChangeListener(mTextWrappingListener);
        mUrlCoordinator.destroy();
        mUrlCoordinator = null;

        mLocationBarLayout.getContext().unregisterComponentCallbacks(mLocationBarMediator);

        mAutocompleteCoordinator.destroy();
        mAutocompleteCoordinator = null;

        mStatusCoordinator.destroy();
        mStatusCoordinator = null;

        mOmniboxDropdownEmbedderImpl.destroy();
        mOmniboxDropdownEmbedderImpl = null;

        if (mFuseboxCoordinator != null) {
            mFuseboxCoordinator.getFuseboxStateSupplier().removeObserver(mOnFuseboxStateChange);
            mFuseboxCoordinator.destroy();
            mFuseboxCoordinator = null;
        }

        mLocationBarLayout.destroy();
        mLocationBarLayout = null;

        mCallbackController.destroy();
        mCallbackController = null;

        mLocationBarMediator.destroy();
        mLocationBarMediator = null;
        GeolocationHeader.stopListeningForLocationUpdates();

        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.setOnZoomLevelChangedCallback(null);
            mPageZoomIndicatorCoordinator.destroy();
            mPageZoomIndicatorCoordinator = null;
        }

        mDestroyed = true;
    }

    @Override
    public void onFinishNativeInitialization() {
        assumeNonNull(mActivityLifecycleDispatcher).unregister(this);
        mActivityLifecycleDispatcher = null;

        mLocationBarMediator.onFinishNativeInitialization();
        mUrlCoordinator.onFinishNativeInitialization();
        mAutocompleteCoordinator.onNativeInitialized();
        mStatusCoordinator.onNativeInitialized();
        if (mPageZoomIndicatorCoordinator != null) {
            mPageZoomIndicatorCoordinator.onNativeInitialized();
        }
        mNativeInitialized = true;
    }

    /**
     * Runs logic that can't be invoked until after native is initialized but shouldn't be on the
     * critical path, e.g. pre-fetching autocomplete suggestions. Contrast with {@link
     * #onFinishNativeInitialization}, which is for logic that should be on the critical path and
     * need native to be initialized. This method must be called after onFinishNativeInitialization.
     */
    @Override
    public void onDeferredStartup() {
        assert mNativeInitialized;
        startAutocompletePrefetch();
    }

    @Override
    public void updateVisualsForState() {
        mLocationBarMediator.updateVisualsForState();
    }

    public void setBookmarkClickListener(OnClickListener listener) {
        mBookmarksButton = mLocationBarLayout.findViewById(R.id.bookmark_button);
        mBookmarksButton.setOnClickListener(
                (view) -> {
                    listener.onClick(view);
                    RecordUserAction.record("MobileToolbarToggleBookmark");
                });
    }

    @Override
    public void setShowTitle(boolean showTitle) {
        mLocationBarMediator.setShowTitle(showTitle);
    }

    @Override
    public void requestUrlBarAccessibilityFocus() {
        mUrlCoordinator.requestAccessibilityFocus();
    }

    @Override
    public void showUrlBarCursorWithoutFocusAnimations() {
        mLocationBarMediator.showUrlBarCursorWithoutFocusAnimations();
    }

    @Override
    public void revertChanges() {
        mLocationBarMediator.revertChanges();
    }

    @Override
    public View getContainerView() {
        return mLocationBarLayout;
    }

    @Override
    public View getSecurityIconView() {
        return mLocationBarLayout.getSecurityIconView();
    }

    /** Returns the {@link VoiceRecognitionHandler} associated with this LocationBar. */
    @Override
    public @Nullable VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mLocationBarMediator.getVoiceRecognitionHandler();
    }

    @Override
    public @Nullable OmniboxStub getOmniboxStub() {
        return mLocationBarMediator;
    }

    @Override
    public UrlBarData getUrlBarData() {
        return mUrlCoordinator.getUrlBarData();
    }

    @Override
    public void addOmniboxSuggestionsDropdownScrollListener(
            OmniboxSuggestionsDropdownScrollListener listener) {
        mAutocompleteCoordinator.addOmniboxSuggestionsDropdownScrollListener(listener);
    }

    @Override
    public void removeOmniboxSuggestionsDropdownScrollListener(
            OmniboxSuggestionsDropdownScrollListener listener) {
        mAutocompleteCoordinator.removeOmniboxSuggestionsDropdownScrollListener(listener);
    }

    @Override
    public void setShowOriginOnly(boolean showOriginOnly) {
        mUrlCoordinator.setShowOriginOnly(showOriginOnly);
    }

    @Override
    public void setUrlBarUsesSmallText(boolean useSmallText) {
        mUrlCoordinator.setUseSmallText(useSmallText);
        mStatusCoordinator.setUseSmallWidget(useSmallText);
    }

    @Override
    public void setShowStatusIconForSecureOrigins(boolean showStatusIconForSecureOrigins) {
        mStatusCoordinator.setShowStatusIconForSecureOrigins(showStatusIconForSecureOrigins);
    }

    @Override
    public void maybeShowOrClearCursorInLocationBar() {
        mLocationBarMediator.maybeShowOrClearCursorInLocationBar();
    }

    // AutocompleteDelegate implementation.
    @Override
    public void onSuggestionsChanged(
            @Nullable AutocompleteMatch defaultMatch, boolean hasSuggestions) {
        assert defaultMatch == null || defaultMatch.allowedToBeDefaultMatch();
        mLocationBarMediator.onSuggestionsChanged(defaultMatch, hasSuggestions);
    }

    @Override
    public void setKeyboardVisibility(boolean shouldShow, boolean delayHide) {
        mUrlCoordinator.setKeyboardVisibility(shouldShow, delayHide);
    }

    @Override
    public boolean isKeyboardActive() {
        return KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(mUrlBar)
                || (mLocationBarLayout.getContext().getResources().getConfiguration().keyboard
                        == Configuration.KEYBOARD_QWERTY);
    }

    @Override
    public void loadUrl(OmniboxLoadUrlParams omniboxLoadUrlParams) {
        mLocationBarMediator.loadUrl(omniboxLoadUrlParams);
    }

    @Override
    public boolean didFocusUrlFromFakebox() {
        return mLocationBarMediator.didFocusUrlFromFakebox();
    }

    @Override
    public void maybeShowDefaultBrowserPromo() {
        mLocationBarMediator.maybeShowDefaultBrowserPromo();
    }

    @Override
    public boolean isToolbarPositionCustomizationEnabled() {
        return mIsToolbarPositionCustomizationEnabled;
    }

    @Override
    public boolean isToolbarBottomAnchored() {
        return mBrowserControlsStateProvider != null
                && mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.BOTTOM;
    }

    @Override
    public void clearOmniboxFocus() {
        mLocationBarMediator.endInput();
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(text), UrlBar.ScrollType.NO_SCROLL, UrlBarData.SELECT_END);
        updateButtonVisibility();
    }

    /**
     * Returns whether the url bar is in the special "focused without animation" state, a special
     * case where we show a blinking cursor in the UrlBar on the NTP in order to accept keyboard
     * input from an attached keyboard, but otherwise do not treat the UrlBar as focused.
     */
    public boolean isUrlBarFocusedWithoutAnimation() {
        return mLocationBarMediator.isUrlBarFocusedWithoutAnimation();
    }

    /**
     * @see UrlBarCoordinator#getVisibleTextPrefixHint()
     */
    public @Nullable CharSequence getOmniboxVisibleTextPrefixHint() {
        return mUrlCoordinator.getVisibleTextPrefixHint();
    }

    /**
     * @see UrlBarCoordinator#getTextWithoutAutocomplete()
     */
    public String getUrlBarTextWithoutAutocomplete() {
        return mUrlCoordinator.getTextWithoutAutocomplete();
    }

    /**
     * @see UrlBarCoordinator#getViewRectProvider()
     */
    public ViewRectProvider getUrlBarViewRectProvider() {
        return mUrlCoordinator.getViewRectProvider();
    }

    /**
     * Returns the {@link LocationBarCoordinatorPhone} for this coordinator.
     *
     * @throws ClassCastException if this coordinator holds a {@link SubCoordinator} of a different
     *     type.
     */
    public LocationBarCoordinatorPhone getPhoneCoordinator() {
        assert mSubCoordinator != null;
        return (LocationBarCoordinatorPhone) mSubCoordinator;
    }

    /**
     * Returns the {@link LocationBarCoordinatorTablet} for this coordinator.
     *
     * @throws ClassCastException if this coordinator holds a {@link SubCoordinator} of a different
     *     type.
     */
    public LocationBarCoordinatorTablet getTabletCoordinator() {
        assert mSubCoordinator != null;
        return (LocationBarCoordinatorTablet) mSubCoordinator;
    }

    public boolean isDestroyed() {
        return mDestroyed;
    }

    /** Initiates a pre-fetch of autocomplete suggestions. */
    public void startAutocompletePrefetch() {
        if (!mNativeInitialized) return;
        mAutocompleteCoordinator.prefetchZeroSuggestResults(
                mLocationBarMediator.getLocationBarDataProvider().getTab());
    }

    /**
     * Updates progress of current the URL focus change animation.
     *
     * @param ntpUrlExpansionFraction The degree to which the omnibox has expanded to full width on
     *     the NTP.
     * @param urlFocusChangeFraction The degree to which the omnibox has expanded due to it is
     *     getting focused.
     */
    public void setUrlFocusChangeFraction(
            float ntpUrlExpansionFraction, float urlFocusChangeFraction) {
        mLocationBarMediator.setUrlFocusChangeFraction(
                ntpUrlExpansionFraction, urlFocusChangeFraction);
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     *
     * <p>Immediately after the animation to transition the URL bar from focused to unfocused
     * finishes, the layout width returned from #getMeasuredWidth() can differ from the final
     * unfocused width (e.g. this value) until the next layout pass is complete.
     *
     * <p>This value may be used to determine whether optional child views should be visible in the
     * unfocused location bar.
     *
     * @param unfocusedWidth The unfocused location bar width.
     */
    public void setUnfocusedWidth(int unfocusedWidth) {
        mLocationBarMediator.setUnfocusedWidth(unfocusedWidth);
    }

    public void setOnSizeChangedRunnable(Runnable onSizeChangedRunnable) {
        mLocationBarLayout.setOnSizeChangedRunnable(onSizeChangedRunnable);
    }

    /** Returns the {@link StatusCoordinator} for the LocationBar. */
    public StatusCoordinator getStatusCoordinator() {
        return mStatusCoordinator;
    }

    /** Returns the {@link UrlBarCoordinator} for the LocationBar. */
    public UrlBarCoordinator getUrlBarCoordinator() {
        return mUrlCoordinator;
    }

    /** Returns the {@link FuseboxCoordinator} for the LocationBar. */
    public FuseboxCoordinator getFuseboxCoordinator() {
        return mFuseboxCoordinator;
    }

    /**
     * @param focusable Whether the url bar should be focusable.
     */
    public void setUrlBarFocusable(boolean focusable) {
        mUrlCoordinator.setAllowFocus(focusable);
    }

    private void onTextWrappingChanged(boolean isWrapping) {
        if (mFuseboxCoordinator != null) {
            mFuseboxCoordinator.onFuseboxTextWrappingChanged(isWrapping);
        }
        mLocationBarMediator.updateButtonVisibility();
    }

    /**
     * Decide if the UrlBar should permit text wrapping.
     *
     * <p>This method instructs the UrlBar to permit text wrapping feature on or off. Not all input
     * is wrapped. The state computed here only decides whether wrapping should be permitted, not
     * whether it will be applied.
     */
    private void updateUrlBarForMultilineInput() {
        boolean allowMultilineInput = OmniboxFeatures.allowMultilineEditField();
        // Disable multiline input on Tablets if Fusebox state is "off".
        allowMultilineInput &= !(isTabletLayout() && mCurrentFuseboxState == FuseboxState.DISABLED);
        mUrlCoordinator.setAllowMultilineInput(allowMultilineInput);
    }

    /* package */ void onFuseboxStateChange(@FuseboxState int newState) {
        if (mUrlCoordinator == null || !mUrlCoordinator.hasFocus()) return;
        View addButton = mLocationBarLayout.findViewById(R.id.location_bar_attachments_add);
        if (addButton == null) return;

        // The Fade and and ChangeBounds anims below are only intended for animating between compact
        // <--> expanded; they don't look good otherwise.
        boolean transitioningFromOrToDisabledState =
                mCurrentFuseboxState == FuseboxState.DISABLED || newState == FuseboxState.DISABLED;

        mCurrentFuseboxState = newState;
        updateUrlBarForMultilineInput();

        if (transitioningFromOrToDisabledState) return;

        ChangeBounds changeBounds = new ChangeBounds();
        changeBounds
                .setDuration(COMPACT_MODE_ANIMATION_DURATION_MS)
                .setInterpolator(Interpolators.STANDARD_INTERPOLATOR)
                .addTarget(mLocationBarLayout)
                .addTarget(addButton);
        Transition transition;
        if (newState == FuseboxState.COMPACT) {
            // Only fade when entering expanded mode.
            transition = changeBounds;
            mLocationBarEmbedder.setRequestFixedHeight(true);
            changeBounds.addListener(
                    new TransitionListenerAdapter() {
                        @Override
                        public void onTransitionCancel(Transition transition) {
                            onTransitionEnd(transition);
                        }

                        @Override
                        public void onTransitionEnd(Transition transition) {
                            mLocationBarEmbedder.setRequestFixedHeight(false);
                        }
                    });
        } else {
            Fade fade = new Fade();
            fade.addTarget(mLocationBarLayout.findViewById(R.id.fusebox_request_type));
            fade.setStartDelay(COMPACT_MODE_FADE_START_DELAY_MS);
            // Delaying the fade prevents the chip from becoming visible before the fusebox expands
            // to include it.
            fade.setDuration(COMPACT_MODE_ANIMATION_DURATION_MS);
            fade.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            transition = new TransitionSet().addTransition(changeBounds).addTransition(fade);
        }
        // If the refactored animations are enabled, the ChangeBounds transition will instead be
        // kicked off with the other transitions in ToolbarPhone.
        if (ChromeFeatureList.sToolbarPhoneAnimationRefactor.isEnabled()) {
            changeBounds.setResizeClip(/* resizeClip= */ true);
            mLocationBarEmbedder.beginEmbeddedDelayedTransition(mLocationBarLayout, transition);
        } else {
            TransitionManager.beginDelayedTransition(mLocationBarLayout, transition);
        }
    }

    /**
     * Triggers a url focus change to begin or end, depending on the value of inProgress.
     *
     * @param inProgress Whether a focus change is in progress.
     */
    public void setUrlFocusChangeInProgress(boolean inProgress) {
        mLocationBarMediator.setUrlFocusChangeInProgress(inProgress);
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change. This will be called after any animations are performed to transition from one focus
     * state to the other.
     *
     * @param showExpandedState Whether the url bar is expanded.
     * @param shouldShowKeyboard Whether the keyboard should be shown. This value is determined by
     *     whether url bar has got focus. Most of the time this is the same as showExpandedState,
     *     but in some cases, e.g. url bar is scrolled to the top of the screen on homepage but not
     *     focused, we set it differently.
     */
    public void finishUrlFocusChange(boolean showExpandedState, boolean shouldShowKeyboard) {
        mLocationBarMediator.finishUrlFocusChange(showExpandedState, shouldShowKeyboard);
    }

    /**
     * Whether the omnibox focus animation should be completed immediately. This is used to put it
     * in a fully expanded state when focusing a bottom-anchored toolbar, avoiding a combination of
     * horizontal and vertical movement in the animation.
     */
    public boolean shouldShortCircuitFocusAnimation(boolean gainingFocus) {
        return gainingFocus && isToolbarBottomAnchored();
    }

    /**
     * Toggles the mic button being shown when the location bar is not focused. By default the mic
     * button is not shown.
     */
    public void setShouldShowMicButtonWhenUnfocused(boolean shouldShowMicButtonWhenUnfocused) {
        mLocationBarMediator.setShouldShowMicButtonWhenUnfocusedForPhone(
                shouldShowMicButtonWhenUnfocused);
    }

    /**
     * Toggles the lens button being shown when the location bar is not focused. By default the lens
     * button is not shown.
     */
    public void setShouldShowLensButtonWhenUnfocused(boolean shouldShowLensButtonWhenUnfocused) {
        mLocationBarMediator.setShouldShowLensButtonWhenUnfocusedForPhone(
                shouldShowLensButtonWhenUnfocused);
    }

    /** Updates the visibility of the buttons inside the location bar. */
    public void updateButtonVisibility() {
        mLocationBarMediator.updateButtonVisibility();
    }

    /** Returns whether the layout is RTL. */
    public boolean isLayoutRtl() {
        return mLocationBarLayout.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
    }

    // Tablet-specific methods.

    /**
     * Returns an animator to run for the given view when hiding buttons in the unfocused location
     * bar. This should also be used to create animators for hiding toolbar buttons.
     *
     * @param button The {@link View} of the button to hide.
     */
    public ObjectAnimator createHideButtonAnimatorForTablet(View button) {
        assert isTabletWindow();
        return mLocationBarMediator.createHideButtonAnimatorForTablet(button);
    }

    /**
     * Returns an animator to run for the given view when showing buttons in the unfocused location
     * bar. This should also be used to create animators for showing toolbar buttons.
     *
     * @param button The {@link View} of the button to show.
     */
    public ObjectAnimator createShowButtonAnimatorForTablet(View button) {
        assert isTabletWindow();
        return mLocationBarMediator.createShowButtonAnimatorForTablet(button);
    }

    /**
     * Creates animators for hiding buttons in the unfocused location bar. The buttons fade out
     * while width of the location bar gets larger. There are toolbar buttons that also hide at the
     * same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding between
     *     the beginning and end of the animation.
     * @return A list of animators to run.
     */
    public List<Animator> getHideButtonsWhenUnfocusedAnimatorsForTablet(
            int toolbarStartPaddingDifference) {
        assert isTabletWindow();
        return mLocationBarMediator.getHideButtonsWhenUnfocusedAnimatorsForTablet(
                toolbarStartPaddingDifference);
    }

    /**
     * Creates animators for showing buttons in the unfocused location bar. The buttons fade in
     * while width of the location bar gets smaller. There are toolbar buttons that also show at the
     * same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding between
     *     the beginning and end of the animation.
     * @return A list of animators to run.
     */
    public List<Animator> getShowButtonsWhenUnfocusedAnimatorsForTablet(
            int toolbarStartPaddingDifference) {
        assert isTabletWindow();
        return mLocationBarMediator.getShowButtonsWhenUnfocusedAnimatorsForTablet(
                toolbarStartPaddingDifference);
    }

    /** Toggles whether buttons should be displayed in the URL bar when it's not focused. */
    public void setShouldShowButtonsWhenUnfocusedForTablet(boolean shouldShowButtons) {
        assert isTabletWindow();
        mLocationBarMediator.setShouldShowButtonsWhenUnfocusedForTablet(shouldShowButtons);
    }

    // End tablet-specific methods.

    public void setVoiceRecognitionHandlerForTesting(
            VoiceRecognitionHandler voiceRecognitionHandler) {
        mLocationBarMediator.setVoiceRecognitionHandlerForTesting(voiceRecognitionHandler);
    }

    public void onUrlChangedForTesting() {
        mLocationBarMediator.onUrlChanged(false);
    }

    public void setLensControllerForTesting(LensController lensController) {
        mLocationBarMediator.setLensControllerForTesting(lensController);
    }

    private boolean isPhoneLayout() {
        return mLocationBarLayout instanceof LocationBarPhone;
    }

    private boolean isTabletLayout() {
        return mLocationBarLayout instanceof LocationBarTablet;
    }

    private boolean isTabletWindow() {
        return DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
    }

    /* package */ LocationBarMediator getMediatorForTesting() {
        return mLocationBarMediator;
    }

    /**
     * @see LocationBarMediator#updateUrlBarHintTextColor(boolean)
     */
    public void updateUrlBarHintTextColor(boolean useDefaultUrlBarHintTextColor) {
        mLocationBarMediator.updateUrlBarHintTextColor(useDefaultUrlBarHintTextColor);
    }

    /**
     * @see LocationBarMediator#updateUrlActionContainerEndMargin(boolean)
     */
    public void updateUrlActionContainerEndMargin(boolean useDefaultUrlActionContainerEndMargin) {
        mLocationBarMediator.updateUrlActionContainerEndMargin(
                useDefaultUrlActionContainerEndMargin);
    }

    public int getUrlActionContainerEndMarginForTesting() {
        return mLocationBarLayout.getUrlActionContainerEndMarginForTesting(); // IN-TEST
    }

    /**
     * @return The location bar's {@link OmniboxSuggestionsVisualState}.
     */
    @Override
    public @Nullable OmniboxSuggestionsVisualState getOmniboxSuggestionsVisualState() {
        return mAutocompleteCoordinator;
    }

    /**
     * Updates the location bar button background.
     *
     * @param backgroundResId The button background resource.
     */
    public void updateButtonBackground(@DrawableRes int backgroundResId) {
        mLocationBarMediator.updateButtonBackground(backgroundResId);
    }

    public void setOnSpecializedFuseboxModeActivatedListener(
            @Nullable Callback<Boolean> onSpecializedFuseboxModeActivatedCallback) {
        mLocationBarMediator.setOnSpecializedFuseboxModeActivatedListener(
                onSpecializedFuseboxModeActivatedCallback);
    }

    public NonNullObservableSupplier<@FuseboxState Integer> getFuseboxStateSupplier() {
        return mFuseboxCoordinator.getFuseboxStateSupplier();
    }

    @Override
    public void onZoomLevelChanged(double zoomLevel) {
        long readableZoomLevel = PageZoomUtils.getReadableZoomLevel(zoomLevel);
        Context context = mLocationBarLayout.getContext();
        String zoomString =
                context.getResources()
                        .getQuantityString(
                                R.plurals.zoom_button_content_description,
                                (int) readableZoomLevel,
                                (int) readableZoomLevel);
        mZoomButton.setContentDescription(zoomString);
        mZoomButton.setTooltipText(zoomString);
        mLocationBarMediator.onZoomLevelChanged();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the bookmark button on the omnibox.
     */
    public ToolbarWidthConsumer getBookmarkButtonToolbarWidthConsumer() {
        return mLocationBarMediator.getBookmarkButtonToolbarWidthConsumer();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the install button on the omnibox.
     */
    public ToolbarWidthConsumer getInstallButtonToolbarWidthConsumer() {
        return mLocationBarMediator.getInstallButtonToolbarWidthConsumer();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the mic button on the omnibox.
     */
    public ToolbarWidthConsumer getMicButtonToolbarWidthConsumer() {
        return mLocationBarMediator.getMicButtonToolbarWidthConsumer();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the lens button on the omnibox.
     */
    public ToolbarWidthConsumer getLensButtonToolbarWidthConsumer() {
        return mLocationBarMediator.getLensButtonToolbarWidthConsumer();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the zoom button on the omnibox.
     */
    public ToolbarWidthConsumer getZoomButtonToolbarWidthConsumer() {
        return mLocationBarMediator.getZoomButtonToolbarWidthConsumer();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the chip on the omnibox in its collapsed (icon only) state.
     */
    public @Nullable ToolbarWidthConsumer getOmniboxChipCollapsedToolbarWidthConsumer() {
        return mLocationBarMediator.getOmniboxChipCollapsedToolbarWidthConsumer();
    }

    /**
     * Returns a {@link ToolbarWidthConsumer} that handles width on the toolbar allocated to showing
     * the chip on the omnibox in its expanded (icon + text) state.
     */
    public @Nullable ToolbarWidthConsumer getOmniboxChipExpandedToolbarWidthConsumer() {
        return mLocationBarMediator.getOmniboxChipExpandedToolbarWidthConsumer();
    }

    /**
     * Apply the X translation to the LocationBar buttons to match the NTP fakebox -> omnibox
     * transition.
     *
     * @param translationX the desired translation to be applied to appropriate LocationBar buttons.
     */
    public void setLocationBarButtonTranslationForNtpAnimation(float translationX) {
        mLocationBarMediator.setLocationBarButtonTranslationForNtpAnimation(translationX);
    }

    /**
     * Set the visibility of the URL action buttons as a whole.
     *
     * <p>Visibility of each button is guarded by two states: visibility of a specific button and
     * visibility of the entire group, ensuring that only requested buttons are shown/hidden when
     * the value passed to this method is toggled.
     */
    public void setUrlActionContainerVisibility(boolean shouldShow) {
        mLocationBarMediator.setUrlActionContainerVisibility(shouldShow);
    }

    /**
     * Set the omnibox to have focus or not.
     *
     * <p>Updates passed AutocompleteInput instance so it correctly reflects the current page URL,
     * title, classification, and focus time, bringing the Fusebox to focus with the supplied data.
     * When null instance is passed the focus is cleared.
     *
     * @param input The AutocompleteInput object with all the details for the focus operation. If
     *     null, the focus will be cleared.
     */
    public void setUrlBarFocus(@Nullable AutocompleteInput input) {
        if (input != null) {
            mLocationBarMediator.beginInput(input);
        } else {
            mLocationBarMediator.endInput();
        }
    }

    /** Set an instance of UrlBarCoordinator for testing. */
    void setUrlCoordinatorForTesting(UrlBarCoordinator urlCoordinator) {
        mUrlCoordinator = urlCoordinator;
    }

    /** Set an instance of LocationBarLayout for testing. */
    void setLocationBarLayoutForTesting(LocationBarLayout locationBarLayout) {
        mLocationBarLayout = locationBarLayout;
    }

    /** Set an instance of LocationBarEmbedder for testing. */
    void setLocationBarEmbedderForTesting(LocationBarEmbedder locationBarEmbedder) {
        mLocationBarEmbedder = locationBarEmbedder;
    }

    /** Set the value of mCurrentFuseboxState for testing. */
    void setCurrentFuseboxStateForTesting(@FuseboxState int state) {
        mCurrentFuseboxState = state;
    }

    /** Returns the value of mCurrentFuseboxState for testing. */
    @FuseboxState
    int getCurrentFuseboxStateForTesting() {
        return mCurrentFuseboxState;
    }

    /**
     * Updates the optional button with the given {@link ButtonData}.
     *
     * <p>This method is only applicable to the phone form factor with the bottom app bar enabled.
     *
     * @param buttonData The {@link ButtonData} to update the optional button with.
     */
    public void updateOptionalButton(@Nullable ButtonData buttonData) {
        if (!isPhoneLayout()
                || !ToolbarVariationUtils.isToolbarUiRefactorEnabled(
                        mLocationBarLayout.getContext())) {
            return;
        }

        assert mUserEducationHelper != null;
        mOptionalButtonData = buttonData;

        View optionalButtonView = mLocationBarLayout.findViewById(R.id.optional_button);
        if (optionalButtonView == null) return;

        if (mOptionalButtonCoordinator == null) {
            mOptionalButtonCoordinator =
                    new OptionalButtonCoordinator(
                            optionalButtonView,
                            () -> assumeNonNull(mUserEducationHelper),
                            mLocationBarLayout,
                            () -> !mLocationBarMediator.isUrlBarFocused(),
                            mTrackerSupplier);

            var context = mLocationBarLayout.getContext();
            mLocationBarMediator.setOptionalButtonColorChangeCallback(
                    mOptionalButtonCoordinator::setIconForegroundColor);

            mOptionalButtonCoordinator.setCollapsedStateWidth(
                    context.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size));
            mOptionalButtonCoordinator.setSuppressCollapsedBackground(true);
            mOptionalButtonCoordinator.setBackgroundColorFilter(
                    SemanticColorUtils.getColorSurface(context));

            // The optional button should hide when the URL bar gains focus and reappear when it
            // loses focus.
            mOptionalButtonUrlFocusChangeListener =
                    new UrlFocusChangeListener() {
                        @Override
                        public void onUrlFocusChange(boolean hasFocus) {
                            updateOptionalButtonState();
                        }
                    };
            mLocationBarMediator.addUrlFocusChangeListener(mOptionalButtonUrlFocusChangeListener);

            // This observer is to detect the transition to/from an NTP which results in the button
            // changing visibility.
            mOptionalButtonLocationBarDataObserver =
                    new LocationBarDataProvider.Observer() {
                        @Override
                        public void onUrlChanged(boolean isTabChanging) {
                            updateOptionalButtonState();
                        }

                        @Override
                        public void onTabChanged(@Nullable Tab previousTab) {
                            updateOptionalButtonState();
                        }
                    };
            mLocationBarMediator
                    .getLocationBarDataProvider()
                    .addObserver(mOptionalButtonLocationBarDataObserver);
        }

        updateOptionalButtonState();
    }

    /** Hides the optional button. */
    public void hideOptionalButton() {
        mOptionalButtonData = null;
        updateOptionalButtonState();
    }

    private void updateOptionalButtonState() {
        if (mOptionalButtonCoordinator == null) return;

        var locationBarDataProvider = mLocationBarMediator.getLocationBarDataProvider();
        boolean isNtp = locationBarDataProvider.getNewTabPageDelegate().isCurrentlyVisible();
        if (!ToolbarVariationUtils.shouldModifyToolbarButtons(
                        mLocationBarLayout.getContext(), isNtp)
                || mLocationBarMediator.isUrlBarFocused()
                || mOptionalButtonData == null) {
            mOptionalButtonCoordinator.hideButton();
        } else {
            mOptionalButtonCoordinator.updateButton(
                    mOptionalButtonData, locationBarDataProvider.isIncognitoBranded());
        }

        updateUrlBarNextFocusForwardId();
    }

    private void updateUrlBarNextFocusForwardId() {
        if (!ToolbarVariationUtils.isToolbarUiRefactorEnabled(mLocationBarLayout.getContext())) {
            mUrlBar.setNextFocusForwardId(R.id.tab_switcher_button);
            return;
        }

        if (mOptionalButtonCoordinator != null
                && mOptionalButtonCoordinator.getViewVisibility() != View.GONE) {
            mUrlBar.setNextFocusForwardId(R.id.optional_button);
        } else if (ToolbarVariationUtils.shouldAppMenuBeInToolbar()) {
            // The app menu is the next button to focus on after the url bar.
            mUrlBar.setNextFocusForwardId(R.id.menu_button);
        } else {
            // There are no more buttons to focus on after the url bar revert to the default
            // behavior.
            mUrlBar.setNextFocusForwardId(View.NO_ID);
        }
    }
}
