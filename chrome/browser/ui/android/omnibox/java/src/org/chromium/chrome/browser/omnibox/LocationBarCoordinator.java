// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.view.ActionMode;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import androidx.annotation.DrawableRes;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.OmniboxUma;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.SaveOfflineButtonState;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator.PageInfoAction;
import org.chromium.chrome.browser.omnibox.status.StatusView;
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
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.Optional;
import java.util.function.BooleanSupplier;

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

    private OmniboxSuggestionsDropdownEmbedderImpl mOmniboxDropdownEmbedderImpl;

    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator {
        /** Destroys SubCoordinator. */
        void destroy();
    }

    /** Downloads page for offline access. */
    public interface OfflineDownloader {
        /**
         * Trigger the download of a page.
         *
         * @param context Context to pull resources from.
         * @param tab Tab containing the page to download.
         * @param fromAppMenu Whether the download is started from the app menu.
         */
        void downloadPage(Context context, @Nullable Tab tab, boolean fromAppMenu);
    }

    private LocationBarLayout mLocationBarLayout;
    private @Nullable SubCoordinator mSubCoordinator;
    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final @Nullable BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final boolean mIsToolbarPositionCustomizationEnabled;
    private UrlBarCoordinator mUrlCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private StatusCoordinator mStatusCoordinator;
    private final WindowAndroid mWindowAndroid;
    private LocationBarMediator mLocationBarMediator;
    private View mUrlBar;
    private @Nullable View mDeleteButton;
    private @Nullable View mMicButton;
    private @Nullable View mLensButton;
    private @Nullable View mBookmarksButton;
    private @Nullable View mSaveOfflineButton;
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
     * @param bringTabToFrontCallback Callback to bring the browser foreground and switch to a tab.
     * @param saveOfflineButtonState Whether the 'save offline' button should be enabled.
     * @param omniboxUma Interface for logging UMA histogram.
     * @param tabWindowManagerSupplier Supplier of glue-level TabWindowManager object.
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
     * @param offlineDownloader Used to initiate download when saveOffline button in url action
     *     container is clicked.
     */
    public LocationBarCoordinator(
            View locationBarLayout,
            View autocompleteAnchorView,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ActionMode.Callback actionModeCallback,
            WindowAndroid windowAndroid,
            Supplier<@Nullable Tab> activityTabSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            BackKeyBehaviorDelegate backKeyBehavior,
            PageInfoAction pageInfoAction,
            Callback<Tab> bringTabToFrontCallback,
            SaveOfflineButtonState saveOfflineButtonState,
            OmniboxUma omniboxUma,
            Supplier<TabWindowManager> tabWindowManagerSupplier,
            BookmarkState bookmarkState,
            BooleanSupplier isToolbarMicEnabledSupplier,
            @Nullable Supplier<MerchantTrustSignalsCoordinator>
                    merchantTrustSignalsCoordinatorSupplier,
            OmniboxActionDelegate omniboxActionDelegate,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            @Nullable BackPressManager backPressManager,
            @Nullable OmniboxSuggestionsDropdownScrollListener
                    omniboxSuggestionsDropdownScrollListener,
            @Nullable ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            LocationBarEmbedderUiOverrides uiOverrides,
            @Nullable View baseChromeLayout,
            Supplier<Integer> bottomWindowPaddingSupplier,
            @Nullable OnLongClickListener onLongClickListener,
            @Nullable BrowserControlsStateProvider browserControlsStateProvider,
            boolean isToolbarPositionCustomizationEnabled,
            OfflineDownloader offlineDownloader) {
        mLocationBarLayout = (LocationBarLayout) locationBarLayout;
        mWindowAndroid = windowAndroid;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mIsToolbarPositionCustomizationEnabled = isToolbarPositionCustomizationEnabled;
        mActivityLifecycleDispatcher.register(this);
        Context context = mLocationBarLayout.getContext();
        OneshotSupplierImpl<TemplateUrlService> templateUrlServiceSupplier =
                new OneshotSupplierImpl<>();
        DeferredIMEWindowInsetApplicationCallback deferredIMEWindowInsetApplicationCallback =
                new DeferredIMEWindowInsetApplicationCallback(
                        () -> mOmniboxDropdownEmbedderImpl.recalculateOmniboxAlignment());
        mOmniboxDropdownEmbedderImpl =
                new OmniboxSuggestionsDropdownEmbedderImpl(
                        mWindowAndroid,
                        autocompleteAnchorView,
                        mLocationBarLayout,
                        uiOverrides.isForcedPhoneStyleOmnibox(),
                        baseChromeLayout,
                        deferredIMEWindowInsetApplicationCallback::getCurrentKeyboardHeight,
                        bottomWindowPaddingSupplier);

        mUrlBar = mLocationBarLayout.findViewById(R.id.url_bar);
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
                        saveOfflineButtonState,
                        omniboxUma,
                        isToolbarMicEnabledSupplier,
                        mOmniboxDropdownEmbedderImpl,
                        tabModelSelectorSupplier,
                        browserControlsStateProvider,
                        offlineDownloader);
        if (backPressManager != null) {
            backPressManager.addHandler(mLocationBarMediator, BackPressHandler.Type.LOCATION_BAR);
        }
        mActivityLifecycleDispatcher.register(mLocationBarMediator);
        final boolean isIncognito =
                incognitoStateProvider != null && incognitoStateProvider.isIncognitoSelected();
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
                        bringTabToFrontCallback,
                        tabWindowManagerSupplier,
                        bookmarkState,
                        omniboxActionDelegate,
                        omniboxSuggestionsDropdownScrollListener,
                        mActivityLifecycleDispatcher,
                        uiOverrides.isForcedPhoneStyleOmnibox(),
                        windowAndroid,
                        deferredIMEWindowInsetApplicationCallback);
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

        mLocationBarMediator.addUrlFocusChangeListener(mAutocompleteCoordinator);
        mLocationBarMediator.addUrlFocusChangeListener(mUrlCoordinator);

        mDeleteButton = mLocationBarLayout.findViewById(R.id.delete_button);
        mDeleteButton.setOnClickListener(mLocationBarMediator::deleteButtonClicked);

        mMicButton = mLocationBarLayout.findViewById(R.id.mic_button);
        mMicButton.setOnClickListener(mLocationBarMediator::micButtonClicked);

        mLensButton = mLocationBarLayout.findViewById(R.id.lens_camera_button);
        mLensButton.setOnClickListener(mLocationBarMediator::lensButtonClicked);

        mSaveOfflineButton = mLocationBarLayout.findViewById(R.id.save_offline_button);
        if (mSaveOfflineButton != null) {
            mSaveOfflineButton.setOnClickListener(mLocationBarMediator::saveOfflineButtonClicked);
        }

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

        if (mBookmarksButton != null) {
            mBookmarksButton.setOnClickListener(null);
            mBookmarksButton = null;
        }

        if (mSaveOfflineButton != null) {
            mSaveOfflineButton.setOnClickListener(null);
            mSaveOfflineButton = null;
        }

        mLocationBarMediator.removeUrlFocusChangeListener(mUrlCoordinator);
        mUrlCoordinator.destroy();
        mUrlCoordinator = null;

        mLocationBarLayout.getContext().unregisterComponentCallbacks(mLocationBarMediator);

        mLocationBarMediator.removeUrlFocusChangeListener(mAutocompleteCoordinator);
        mAutocompleteCoordinator.destroy();
        mAutocompleteCoordinator = null;

        mStatusCoordinator.destroy();
        mStatusCoordinator = null;

        mLocationBarLayout.destroy();
        mLocationBarLayout = null;

        mCallbackController.destroy();
        mCallbackController = null;

        mLocationBarMediator.destroy();
        mLocationBarMediator = null;
        GeolocationHeader.stopListeningForLocationUpdates();

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
    }

    @Override
    public void setHideStatusIconForSecureOrigins(boolean hideStatusIconForSecureOrigins) {
        mStatusCoordinator.setHideStatusIconForSecureOrigins(hideStatusIconForSecureOrigins);
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
        return KeyboardVisibilityDelegate.getInstance()
                        .isKeyboardShowing(mLocationBarLayout.getContext(), mUrlBar)
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
        mAutocompleteCoordinator.prefetchZeroSuggestResults();
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

    /**
     * @param show Whether the status icon background should be shown.
     */
    public void setStatusIconBackgroundVisibility(boolean show) {
        mStatusCoordinator.setStatusIconBackgroundVisibility(show);
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
    public @Nullable ObjectAnimator createHideButtonAnimatorForTablet(View button) {
        assert isTabletWindow();

        if (mLocationBarMediator != null) {
            return mLocationBarMediator.createHideButtonAnimatorForTablet(button);
        } else {
            return null;
        }
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
        mLocationBarMediator.onUrlChanged();
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
    public Optional<OmniboxSuggestionsVisualState> getOmniboxSuggestionsVisualState() {
        return Optional.of(mAutocompleteCoordinator);
    }

    /**
     * Updates the location bar button background.
     *
     * @param backgroundResId The button background resource.
     */
    public void updateButtonBackground(@DrawableRes int backgroundResId) {
        mLocationBarMediator.updateButtonBackground(backgroundResId);
    }
}
