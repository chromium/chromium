// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.view.ActionMode;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.OmniboxUma;
import org.chromium.chrome.browser.omnibox.LocationBarMediator.SaveOfflineButtonState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator.PageInfoAction;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.base.HistoryClustersProcessor.OpenHistoryClustersDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * The public API of the location bar component. Location bar responsibilities are:
 * <ul>
 *   <li>Display the current URL.
 *   <li>Display Status.
 *   <li>Handle omnibox input.
 * </ul>
 *
 * <p>The coordinator creates and owns elements within this component.
 */

public class LocationBarCoordinator
        implements LocationBar, NativeInitObserver, AutocompleteDelegate {
    private final OmniboxSuggestionsDropdownEmbedderImpl mOmniboxDropdownEmbedderImpl;

    /** Identifies coordinators with methods specific to a device type. */
    public interface SubCoordinator {
        /** Destroys SubCoordinator. */
        void destroy();
    }

    private LocationBarLayout mLocationBarLayout;
    @Nullable
    private SubCoordinator mSubCoordinator;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private UrlBarCoordinator mUrlCoordinator;
    private AutocompleteCoordinator mAutocompleteCoordinator;
    private StatusCoordinator mStatusCoordinator;
    private WindowDelegate mWindowDelegate;
    private WindowAndroid mWindowAndroid;
    private View mAutocompleteAnchorView;
    private LocationBarMediator mLocationBarMediator;
    private View mUrlBar;
    private View mDeleteButton;
    private View mMicButton;
    private View mLensButton;
    private CallbackController mCallbackController = new CallbackController();
    private boolean mDestroyed;

    private boolean mNativeInitialized;
    private final int mDropdownStandardBackgroundColor;
    private final int mDropdownIncognitoBackgroundColor;
    private final int mSuggestionStandardBackgroundColor;
    private final int mSuggestionIncognitoBackgroundColor;

    /**
     * Creates {@link LocationBarCoordinator} and its subcoordinator: {@link
     * LocationBarCoordinatorPhone} or {@link LocationBarCoordinatorTablet}, depending on the type
     * of {@code locationBarLayout}; no sub-coordinator is created for other LocationBarLayout
     * subclasses.
     * {@code LocationBarCoordinator} owns the subcoordinator. Destroying the former destroys the
     * latter.
     *
     * @param locationBarLayout Inflated {@link LocationBarLayout}.
     *         {@code LocationBarCoordinator} takes ownership and will destroy this object.
     * @param profileObservableSupplier The supplier of the active profile.
     * @param privacyPreferencesManager Privacy preference settings manager.
     * @param locationBarDataProvider {@link LocationBarDataProvider} to be used for accessing
     *         Toolbar state.
     * @param actionModeCallback The default callback for text editing action bar to use.
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param activityTabSupplier A Supplier to access the activity's current tab.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} object.
     * @param shareDelegateSupplier A supplier for {@link ShareDelegate} object.
     * @param incognitoStateProvider An {@link IncognitoStateProvider} to access the current
     *         incognito state.
     * @param activityLifecycleDispatcher Allows observation of the activity state.
     * @param overrideUrlLoadingDelegate Delegate that allows customization of url loading behavior.
     * @param backKeyBehavior Delegate that allows customization of back key behavior.
     * @param searchEngineLogoUtils Utils to query the state of the search engine logos feature.
     * @param pageInfoAction Displays page info popup.
     * @param bringTabToFrontCallback Callback to bring the browser foreground and switch to a tab.
     * @param saveOfflineButtonState Whether the 'save offline' button should be enabled.
     * @param omniboxUma Interface for logging UMA histogram.
     * @param tabWindowManagerSupplier Supplier of glue-level TabWindowManager object.
     * @param bookmarkState State of a URL bookmark state.
     * @param isToolbarMicEnabledSupplier Whether toolbar mic is enabled or not.
     * @param merchantTrustSignalsCoordinatorSupplier Supplier of {@link
     *         MerchantTrustSignalsCoordinator}. Can be null if a store icon shouldn't be shown,
     *         such as when called from a search activity.
     * @param reportExceptionCallback A {@link Callback} to report exceptions.
     * @param backPressManager The {@link BackPressManager} for intercepting back press.
     */
    public LocationBarCoordinator(View locationBarLayout, View autocompleteAnchorView,
            ObservableSupplier<Profile> profileObservableSupplier,
            PrivacyPreferencesManager privacyPreferencesManager,
            LocationBarDataProvider locationBarDataProvider, ActionMode.Callback actionModeCallback,
            WindowDelegate windowDelegate, WindowAndroid windowAndroid,
            @NonNull Supplier<Tab> activityTabSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            IncognitoStateProvider incognitoStateProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            BackKeyBehaviorDelegate backKeyBehavior, SearchEngineLogoUtils searchEngineLogoUtils,
            @NonNull PageInfoAction pageInfoAction, @NonNull Callback<Tab> bringTabToFrontCallback,
            @NonNull SaveOfflineButtonState saveOfflineButtonState, @NonNull OmniboxUma omniboxUma,
            @NonNull Supplier<TabWindowManager> tabWindowManagerSupplier,
            @NonNull BookmarkState bookmarkState,
            @NonNull BooleanSupplier isToolbarMicEnabledSupplier,
            @Nullable Supplier<MerchantTrustSignalsCoordinator>
                    merchantTrustSignalsCoordinatorSupplier,
            @NonNull OmniboxActionDelegate omniboxActionDelegate,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            Callback<Throwable> reportExceptionCallback,
            @Nullable BackPressManager backPressManager,
            @NonNull OmniboxSuggestionsDropdownScrollListener
                    omniboxSuggestionsDropdownScrollListener,
            @Nullable OpenHistoryClustersDelegate openHistoryClustersDelegate) {
        mLocationBarLayout = (LocationBarLayout) locationBarLayout;
        mWindowDelegate = windowDelegate;
        mWindowAndroid = windowAndroid;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        mAutocompleteAnchorView = autocompleteAnchorView;
        Context context = mLocationBarLayout.getContext();
        OneshotSupplierImpl<TemplateUrlService> templateUrlServiceSupplier =
                new OneshotSupplierImpl<>();
        mOmniboxDropdownEmbedderImpl = new OmniboxSuggestionsDropdownEmbedderImpl(
                mWindowAndroid, mWindowDelegate, autocompleteAnchorView, mLocationBarLayout);

        mUrlBar = mLocationBarLayout.findViewById(R.id.url_bar);
        // TODO(crbug.com/1151513): Inject LocaleManager instance to LocationBarCoordinator instead
        // of using the singleton.
        mLocationBarMediator = new LocationBarMediator(context, mLocationBarLayout,
                locationBarDataProvider, profileObservableSupplier, privacyPreferencesManager,
                overrideUrlLoadingDelegate, LocaleManager.getInstance(), templateUrlServiceSupplier,
                backKeyBehavior, windowAndroid, isTabletWindow() && isTabletLayout(),
                searchEngineLogoUtils, LensController.getInstance(), saveOfflineButtonState,
                omniboxUma, isToolbarMicEnabledSupplier, mOmniboxDropdownEmbedderImpl);
        if (backPressManager != null && BackPressManager.isEnabled()) {
            backPressManager.addHandler(mLocationBarMediator, BackPressHandler.Type.LOCATION_BAR);
        }
        final boolean isIncognito =
                incognitoStateProvider != null && incognitoStateProvider.isIncognitoSelected();
        mUrlCoordinator =
                new UrlBarCoordinator((UrlBar) mUrlBar, windowDelegate, actionModeCallback,
                        mCallbackController.makeCancelable(mLocationBarMediator::onUrlFocusChange),
                        mLocationBarMediator, windowAndroid.getKeyboardDelegate(), isIncognito,
                        reportExceptionCallback);
        mAutocompleteCoordinator = new AutocompleteCoordinator(mLocationBarLayout,
                AutocompleteControllerProvider.from(windowAndroid), this,
                mOmniboxDropdownEmbedderImpl, mUrlCoordinator, modalDialogManagerSupplier,
                activityTabSupplier, shareDelegateSupplier, locationBarDataProvider,
                profileObservableSupplier, bringTabToFrontCallback, tabWindowManagerSupplier,
                bookmarkState, omniboxActionDelegate, omniboxSuggestionsDropdownScrollListener,
                openHistoryClustersDelegate);
        StatusView statusView = mLocationBarLayout.findViewById(R.id.location_bar_status);
        mStatusCoordinator = new StatusCoordinator(isTabletWindow(), statusView, mUrlCoordinator,
                locationBarDataProvider, templateUrlServiceSupplier, searchEngineLogoUtils,
                profileObservableSupplier, windowAndroid, pageInfoAction,
                merchantTrustSignalsCoordinatorSupplier, browserControlsVisibilityDelegate);
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

        mUrlBar.setOnKeyListener(mLocationBarMediator);
        mUrlCoordinator.addUrlTextChangeListener(mAutocompleteCoordinator);
        // The LocationBar's direction is tied to the UrlBar's text direction. Icons inside the
        // location bar, e.g. lock, refresh, X, should be reversed if UrlBar's text is RTL.
        mUrlCoordinator.setUrlDirectionListener(
                mCallbackController.makeCancelable(layoutDirection -> {
                    ViewCompat.setLayoutDirection(mLocationBarLayout, (Integer) layoutDirection);
                    mAutocompleteCoordinator.updateSuggestionListLayoutDirection();
                }));

        context.registerComponentCallbacks(mLocationBarMediator);
        mLocationBarLayout.initialize(mAutocompleteCoordinator, mUrlCoordinator, mStatusCoordinator,
                locationBarDataProvider, searchEngineLogoUtils);

        mDropdownStandardBackgroundColor = ChromeColors.getSurfaceColor(
                context, R.dimen.omnibox_suggestion_dropdown_bg_elevation);
        int incognitoBgColorRes = ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                ? R.color.default_bg_color_dark_elev_1_gm3_baseline
                : R.color.omnibox_dropdown_bg_incognito;
        mDropdownIncognitoBackgroundColor = context.getColor(incognitoBgColorRes);
        mSuggestionStandardBackgroundColor =
                OmniboxResourceProvider.getStandardSuggestionBackgroundColor(context);
        mSuggestionIncognitoBackgroundColor =
                context.getColor(R.color.omnibox_suggestion_bg_incognito);

        Callback<Profile> profileObserver = new Callback<>() {
            @Override
            public void onResult(Profile profile) {
                templateUrlServiceSupplier.set(TemplateUrlServiceFactory.getForProfile(profile));
                profileObservableSupplier.removeObserver(this);
            }
        };
        profileObservableSupplier.addObserver(profileObserver);

        if (isPhoneLayout()) {
            mSubCoordinator = new LocationBarCoordinatorPhone(
                    (LocationBarPhone) locationBarLayout, mStatusCoordinator);
        } else if (isTabletLayout()) {
            mSubCoordinator =
                    new LocationBarCoordinatorTablet((LocationBarTablet) locationBarLayout);
        }
        // There is a third possibility: SearchActivityLocationBarLayout extends LocationBarLayout
        // and can be instantiated on phones *or* tablets.
    }

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

        mDestroyed = true;
    }

    @Override
    public void onFinishNativeInitialization() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;

        mLocationBarMediator.onFinishNativeInitialization();
        mUrlCoordinator.onFinishNativeInitialization();
        mAutocompleteCoordinator.onNativeInitialized();
        mStatusCoordinator.onNativeInitialized();
        mNativeInitialized = true;
    }

    /**
     * Runs logic that can't be invoked until after native is initialized but shouldn't be on the
     * critical path, e.g. pre-fetching autocomplete suggestions. Contrast with
     * {@link #onFinishNativeInitialization}, which is for logic that should be on the critical path
     * and need native to be initialized. This method must be called after
     * onFinishNativeInitialization.
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
    @Nullable
    @Override
    public VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mLocationBarMediator.getVoiceRecognitionHandler();
    }

    @Nullable
    @Override
    public OmniboxStub getOmniboxStub() {
        return mLocationBarMediator;
    }

    @Override
    public UrlBarData getUrlBarData() {
        return mUrlCoordinator.getUrlBarData();
    }

    // AutocompleteDelegate implementation.
    @Override
    public void onUrlTextChanged() {
        mLocationBarMediator.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsChanged(String autocompleteText, boolean defaultMatchIsSearch) {
        mLocationBarMediator.onSuggestionsChanged(autocompleteText, defaultMatchIsSearch);
    }

    @Override
    public void setKeyboardVisibility(boolean shouldShow, boolean delayHide) {
        mUrlCoordinator.setKeyboardVisibility(shouldShow, delayHide);
    }

    @Override
    public boolean isKeyboardActive() {
        return KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                       mLocationBarLayout.getContext(), mUrlBar)
                || (mLocationBarLayout.getContext().getResources().getConfiguration().keyboard
                        == Configuration.KEYBOARD_QWERTY);
    }

    @Override
    public void loadUrl(String url, int transition, long inputStart) {
        mLocationBarMediator.loadUrl(url, transition, inputStart);
    }

    @Override
    public void loadUrlWithPostData(
            String url, int transition, long inputStart, String postDataType, byte[] postData) {
        mLocationBarMediator.loadUrlWithPostData(
                url, transition, inputStart, postDataType, postData);
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
    public void clearOmniboxFocus() {
        mLocationBarMediator.clearOmniboxFocus();
    }

    @Override
    public void setOmniboxEditingText(String text) {
        mUrlCoordinator.setUrlBarData(UrlBarData.forNonUrlText(text), UrlBar.ScrollType.NO_SCROLL,
                UrlBarCoordinator.SelectionState.SELECT_END);
        updateButtonVisibility();
    }

    @Override
    public boolean shouldClearOmniboxOnFocus() {
        return mLocationBarMediator.shouldClearOmniboxOnFocus();
    }

    /** @see UrlBarCoordinator#getVisibleTextPrefixHint() */
    public CharSequence getOmniboxVisibleTextPrefixHint() {
        return mUrlCoordinator.getVisibleTextPrefixHint();
    }

    /** @see UrlBarCoordinator#getTextWithoutAutocomplete() */
    public String getUrlBarTextWithoutAutocomplete() {
        return mUrlCoordinator.getTextWithoutAutocomplete();
    }

    /**
     * Returns the {@link LocationBarCoordinatorPhone} for this coordinator.
     *
     * @throws ClassCastException if this coordinator holds a {@link SubCoordinator} of a different
     *         type.
     */
    public @NonNull LocationBarCoordinatorPhone getPhoneCoordinator() {
        assert mSubCoordinator != null;
        return (LocationBarCoordinatorPhone) mSubCoordinator;
    }

    /**
     * Returns the {@link LocationBarCoordinatorTablet} for this coordinator.
     *
     * @throws ClassCastException if this coordinator holds a {@link SubCoordinator} of a different
     *         type.
     */
    public @NonNull LocationBarCoordinatorTablet getTabletCoordinator() {
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
     * @param fraction 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangeFraction(float fraction) {
        mLocationBarMediator.setUrlFocusChangeFraction(fraction);
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

    /**
     * @param focusable Whether the url bar should be focusable.
     */
    public void setUrlBarFocusable(boolean focusable) {
        mUrlCoordinator.setAllowFocus(focusable);
    }

    /**
     * Triggers a url focus change to begin or end, depending on the value of inProgress.
     * @param inProgress Whether a focus change is in progress.
     */
    public void setUrlFocusChangeInProgress(boolean inProgress) {
        mLocationBarMediator.setUrlFocusChangeInProgress(inProgress);
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change. This will be called after any animations are performed to transition from one
     * focus state to the other.
     *
     * @param showExpandedState Whether the url bar is expanded.
     * @param shouldShowKeyboard Whether the keyboard should be shown. This value is determined by
     *         whether url bar has got focus. Most of the time this is the same as
     *         showExpandedState, but in some cases, e.g. url bar is scrolled to the top of the
     *         screen on homepage but not focused, we set it differently.
     * @param shouldShowInOverviewMode Whether the location bar should be shown when in overview
     *         mode.
     */
    public void finishUrlFocusChange(boolean showExpandedState, boolean shouldShowKeyboard,
            boolean shouldShowInOverviewMode) {
        mLocationBarMediator.finishUrlFocusChange(showExpandedState, shouldShowKeyboard);
        if (shouldShowInOverviewMode) {
            mStatusCoordinator.onSecurityStateChanged();
        }
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

    /** @param show Whether the status icon background should be shown. */
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
     * while width of the location bar gets larger. There are toolbar buttons that also hide at
     * the same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding
     *         between the beginning and end of the animation.
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
     * while width of the location bar gets smaller. There are toolbar buttons that also show at
     * the same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding
     *         between the beginning and end of the animation.
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

    /* package */ StatusCoordinator getStatusCoordinatorForTesting() {
        return mStatusCoordinator;
    }

    /**
     * @param isIncognito Whether we are currently in incognito mode.
     * @return The  background color for the Omnibox suggestion dropdown list.
     */
    public int getDropdownBackgroundColor(boolean isIncognito) {
        return isIncognito ? mDropdownIncognitoBackgroundColor : mDropdownStandardBackgroundColor;
    }

    /**
     * @param isIncognito Whether we are currently in incognito mode.
     * @return The the background color for each individual suggestion.
     */
    public int getSuggestionBackgroundColor(boolean isIncognito) {
        return isIncognito ? mSuggestionIncognitoBackgroundColor
                           : mSuggestionStandardBackgroundColor;
    }

    /**
     * Function used to position the url bar inside the location bar during omnibox animation.
     *
     * @param urlExpansionPercent The current expansion percent, 1 is fully focused and 0 is
     *                            completely unfocused.
     * @param isOnNtp True if tab mode is the NTP.
     * @return The X translation for the URL bar, used in the toolbar animation.
     */
    public float getUrlBarTranslationXForToolbarAnimation(
            float urlExpansionPercent, boolean isOnNtp) {
        return mLocationBarMediator.getUrlBarTranslationXForToolbarAnimation(
                urlExpansionPercent, isOnNtp);
    }
}
