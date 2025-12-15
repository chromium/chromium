// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.view.ActionMode;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.WindowInsets;

import androidx.annotation.DrawableRes;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
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
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.accessibility.PageZoomIndicatorCoordinator;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
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

    private static final long COMPACT_MODE_ANIMATION_DURATION_MS = 200;
    private final DeferredIMEWindowInsetApplicationCallback
            mDeferredIMEWindowInsetApplicationCallback;

    private OmniboxSuggestionsDropdownEmbedderImpl mOmniboxDropdownEmbedderImpl;

    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator {
        /** Destroys SubCoordinator. */
        void destroy();
    }

    private LocationBarLayout mLocationBarLayout;
    private @Nullable SubCoordinator mSubCoordinator;
    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final LocationBarEmbedder mLocationBarEmbedder;
    private final @Nullable BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final boolean mIsToolbarPositionCustomizationEnabled;
    private final View mBottomContainerView;
    private UrlBarCoordinator mUrlCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private StatusCoordinator mStatusCoordinator;
    private FuseboxCoordinator mFuseboxCoordinator;
    private final WindowAndroid mWindowAndroid;
    private final Callback<Boolean> mTextWrappingListener;
    private LocationBarMediator mLocationBarMediator;
    private View mUrlBar;
    private View mZoomButton;
    private @Nullable View mDeleteButton;
    private @Nullable View mNavigateButton;
    private @Nullable View mMicButton;
    private @Nullable View mLensButton;
    private @Nullable View mComposeplateButton;
    private @Nullable View mBookmarksButton;
    private @Nullable View mInstallButton;
    private @Nullable PageZoomIndicatorCoordinator mPageZoomIndicatorCoordinator;
    private CallbackController mCallbackController = new CallbackController();
    private boolean mDestroyed;

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
     * @param merchantTrustSignalsCoordinatorSupplier Supplier of {@link
     *     MerchantTrustSignalsCoordinator}. Can be null if a store icon shouldn't be shown, such as
     *     when called from a search activity.
     * @param backPressManager The {@link BackPressManager} for intercepting back press.
     * @param tabModelSelectorSupplier Supplier of the {@link TabModelSelector}.
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
     * @param pageZoomManager The {@link PageZoomManager} for managing the page zoom.
     */
    public LocationBarCoordinator(
            View locationBarLayout,
            View autocompleteAnchorView,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ActionMode.@Nullable Callback actionModeCallback,
            WindowAndroid windowAndroid,
            Supplier<@Nullable Tab> activityTabSupplier,
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
            @Nullable Supplier<MerchantTrustSignalsCoordinator>
                    merchantTrustSignalsCoordinatorSupplier,
            OmniboxActionDelegate omniboxActionDelegate,
            @Nullable BrowserStateBrowserControlsVisibilityDelegate
                    browserControlsVisibilityDelegate,
            @Nullable BackPressManager backPressManager,
            @Nullable OmniboxSuggestionsDropdownScrollListener
                    omniboxSuggestionsDropdownScrollListener,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            LocationBarEmbedder locationBarEmbedder,
            LocationBarEmbedderUiOverrides uiOverrides,
            @Nullable View baseChromeLayout,
            Supplier<Integer> bottomWindowPaddingSupplier,
            @Nullable OnLongClickListener onLongClickListener,
            @Nullable BrowserControlsStateProvider browserControlsStateProvider,
            boolean isToolbarPositionCustomizationEnabled,
            @Nullable PageZoomManager pageZoomManager,
            Function<Tab, @Nullable Bitmap> tabFaviconFunction,
            @Nullable MultiInstanceManager multiInstanceManager,
            SnackbarManager snackbarManager,
            View bottomContainerView) {
        mLocationBarLayout = (LocationBarLayout) locationBarLayout;
        mWindowAndroid = windowAndroid;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mLocationBarEmbedder = locationBarEmbedder;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mIsToolbarPositionCustomizationEnabled = isToolbarPositionCustomizationEnabled;
        mBottomContainerView = bottomContainerView;
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
        mOmniboxDropdownEmbedderImpl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mWindowAndroid,
                        autocompleteAnchorView,
                        mLocationBarLayout,
                        uiOverrides.isForcedPhoneStyleOmnibox(),
                        baseChromeLayout,
                        () ->
                                mBrowserControlsStateProvider == null
                                        ? ControlsPosition.TOP
                                        : mBrowserControlsStateProvider.getControlsPosition(),
                        mDeferredIMEWindowInsetApplicationCallback::getCurrentKeyboardHeight,
                        bottomWindowPaddingSupplier,
                        locationBarDataProvider);

        mUrlBar = mLocationBarLayout.findViewById(R.id.url_bar);
        final boolean isIncognito =
                incognitoStateProvider != null && incognitoStateProvider.isIncognitoSelected();
        OmniboxResourceProvider.setTabFaviconFactory(tabFaviconFunction);
        ObservableSupplierImpl<@AutocompleteRequestType Integer> autocompleteRequestTypeSupplier =
                new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
        mFuseboxCoordinator =
                new FuseboxCoordinator(
                        context,
                        windowAndroid,
                        mLocationBarLayout,
                        profileObservableSupplier,
                        locationBarDataProvider,
                        tabModelSelectorSupplier,
                        templateUrlServiceSupplier,
                        autocompleteRequestTypeSupplier,
                        snackbarManager);
        if (OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()) {
            mFuseboxCoordinator.getFuseboxStateSupplier().addObserver(this::onCompactModeChange);
        }

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
                        autocompleteRequestTypeSupplier,
                        mPageZoomIndicatorCoordinator,
                        mFuseboxCoordinator,
                        multiInstanceManager,
                        locationBarEmbedder);
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
                        onLongClickListener);

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
                        mUrlCoordinator,
                        locationBarDataProvider,
                        templateUrlServiceSupplier,
                        profileObservableSupplier,
                        windowAndroid,
                        pageInfoAction,
                        merchantTrustSignalsCoordinatorSupplier,
                        browserControlsVisibilityDelegate);
        mLocationBarMediator.setCoordinators(
                mUrlCoordinator, mAutocompleteCoordinator, mStatusCoordinator);
        mLocationBarMediator.addUrlFocusChangeListener(mFuseboxCoordinator);
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

        if (ChromeFeatureList.sAndroidComposeplate.isEnabled()
                && !ChromeFeatureList.sAndroidComposeplateV2Enabled.getValue()) {
            mComposeplateButton = mLocationBarLayout.findViewById(R.id.composeplate_button);
            mComposeplateButton.setOnClickListener(mLocationBarMediator::composeplateButtonClicked);
        }

        mZoomButton = mLocationBarLayout.findViewById(R.id.zoom_button);
        mZoomButton.setOnClickListener(mLocationBarMediator::zoomButtonClicked);

        mInstallButton = mLocationBarLayout.findViewById(R.id.install_button);
        mInstallButton.setOnClickListener(mLocationBarMediator::installButtonClicked);

        mUrlCoordinator.setTextChangeListener(mAutocompleteCoordinator::onTextChanged);
        mUrlCoordinator.setKeyDownListener(mLocationBarMediator);
        mUrlCoordinator.setTypingStartedListener(
                mLocationBarMediator::completeUrlFocusAnimationAndEnableSuggestions);

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
                locationBarDataProvider);

        Callback<Profile> profileObserver =
                new Callback<>() {
                    @Override
                    public void onResult(Profile profile) {
                        templateUrlServiceSupplier.set(
                                TemplateUrlServiceFactory.getForProfile(profile));
                        profileObservableSupplier.removeObserver(this);
                    }
                };
        profileObservableSupplier.addObserver(profileObserver);

        if (isPhoneLayout()) {
            mSubCoordinator =
                    new LocationBarCoordinatorPhone(
                            (LocationBarPhone) locationBarLayout, mStatusCoordinator);
        } else if (isTabletLayout()) {
            mSubCoordinator =
                    new LocationBarCoordinatorTablet((LocationBarTablet) locationBarLayout);
        }
        // There is a third possibility: SearchActivityLocationBarLayout extends LocationBarLayout
        // and can be instantiated on phones *or* tablets.
    }

    private void updateBottomContainerPosition() {
        var layoutParams = (MarginLayoutParams) mBottomContainerView.getLayoutParams();
        if (isUrlBarFocused()) {
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

        mMicButton.setOnClickListener(null);
        mMicButton = null;

        mLensButton.setOnClickListener(null);
        mLensButton = null;

        if (mComposeplateButton != null) {
            mComposeplateButton.setOnClickListener(null);
            mComposeplateButton = null;
        }

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

        mLocationBarMediator.removeUrlFocusChangeListener(mUrlCoordinator);
        mUrlCoordinator.removeTextWrappingChangeListener(mTextWrappingListener);
        mUrlCoordinator.destroy();
        mUrlCoordinator = null;

        mLocationBarLayout.getContext().unregisterComponentCallbacks(mLocationBarMediator);

        mAutocompleteCoordinator.destroy();
        mAutocompleteCoordinator = null;

        mStatusCoordinator.destroy();
        mStatusCoordinator = null;

        if (mFuseboxCoordinator != null) {
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
    public void clearUrlBarCursorWithoutFocusAnimations() {
        mLocationBarMediator.clearUrlBarCursorWithoutFocusAnimations();
    }

    @Override
    public void selectAll() {
        mUrlCoordinator.selectAll();
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
    public void onUrlTextChanged() {
        mLocationBarMediator.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsChanged(@Nullable AutocompleteMatch defaultMatch) {
        assert defaultMatch == null || defaultMatch.allowedToBeDefaultMatch();
        mLocationBarMediator.onSuggestionsChanged(defaultMatch);
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
    public boolean isUrlBarFocused() {
        return mLocationBarMediator.isUrlBarFocused();
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
        mLocationBarMediator.clearOmniboxFocus();
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mUrlCoordinator.setUrlBarData(
                UrlBarData.forNonUrlText(text),
                UrlBar.ScrollType.NO_SCROLL,
                UrlBarCoordinator.SelectionState.SELECT_END);
        updateButtonVisibility();
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

    /** Returns the {@link StatusCoordinator} for the LocationBar. */
    public StatusCoordinator getStatusCoordinator() {
        return mStatusCoordinator;
    }

    /** Returns the {@link UrlBarCoordinator} for the LocationBar. */
    public UrlBarCoordinator getUrlBarCoordinator() {
        return mUrlCoordinator;
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

    private void onCompactModeChange(@FuseboxState int state) {
        if (!mUrlCoordinator.hasFocus()) return;
        View addButton = mLocationBarLayout.findViewById(R.id.location_bar_attachments_add);
        if (addButton == null) return;

        ChangeBounds changeBounds = new ChangeBounds();
        changeBounds
                .setDuration(COMPACT_MODE_ANIMATION_DURATION_MS)
                .addTarget(mLocationBarLayout)
                .addTarget(addButton);
        if (state == FuseboxState.COMPACT) {
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
        }
        TransitionManager.beginDelayedTransition(mLocationBarLayout, changeBounds);
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

    public ObservableSupplier<@AutocompleteRequestType Integer>
            getAutocompleteRequestTypeSupplier() {
        return mLocationBarMediator.getAutocompleteRequestTypeSupplier();
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
}
