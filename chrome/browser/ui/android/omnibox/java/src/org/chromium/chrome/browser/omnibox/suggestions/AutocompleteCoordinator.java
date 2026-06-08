// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.content.Context;
import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteStopReason;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.KeyNavigationUtil;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Coordinator that handles the interactions with the autocomplete system. */
@NullMarked
public class AutocompleteCoordinator implements OmniboxSuggestionsVisualState {
    private final ViewGroup mParent;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileChangeCallback;
    private final AutocompleteMediator mMediator;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private @Nullable OmniboxSuggestionsContainer mContainer;
    private @Nullable OmniboxSuggestionsDropdown mDropdown;
    private final ObserverList<OmniboxSuggestionsDropdownScrollListener> mScrollListenerList =
            new ObserverList<>();
    private final SuggestionListViewHolderProvider mViewProvider;
    private final LocationBarEmbedder mLocationBarEmbedder;
    private boolean mDropdownAvailableRecorded;
    private final @Nullable OmniboxViewHolderFactory mViewHolderFactory;
    private final @Nullable PreWarmingRecycledViewPool mRecycledViewPool;

    /** An observer watching for changes to the visual state of the omnibox suggestions. */
    public interface OmniboxSuggestionsVisualStateObserver {
        /** Called when the Omnibox session state changes. */
        void onOmniboxSessionStateChange(boolean isActive);

        /** Called when the background color of the omnibox suggestions changes. */
        void onOmniboxSuggestionsBackgroundColorChanged(@ColorInt int color);
    }

    public AutocompleteCoordinator(
            ViewGroup parent,
            AutocompleteDelegate delegate,
            OmniboxSuggestionsDropdownEmbedder dropdownEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            Supplier<@Nullable Tab> activityTabSupplier,
            @Nullable Supplier<ShareDelegate> shareDelegateSupplier,
            LocationBarDataProvider locationBarDataProvider,
            LocationBarEmbedder locationBarEmbedder,
            MonotonicObservableSupplier<Profile> profileObservableSupplier,
            Callback<String> bringTabGroupToForegroundCallback,
            BookmarkState bookmarkState,
            OmniboxActionDelegateImpl omniboxActionDelegate,
            @Nullable OmniboxSuggestionsDropdownScrollListener scrollListener,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            boolean forcePhoneStyleOmnibox,
            WindowAndroid windowAndroid,
            DeferredIMEWindowInsetApplicationCallback deferredIMEWindowInsetApplicationCallback,
            FuseboxCoordinator fuseboxCoordinator) {
        mParent = parent;
        mLocationBarEmbedder = locationBarEmbedder;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        Context context = parent.getContext();

        ModelList listItems = new ModelList();
        PropertyModel listModel =
                new PropertyModel.Builder(SuggestionListProperties.ALL_KEYS)
                        .with(SuggestionListProperties.EMBEDDER, dropdownEmbedder)
                        .with(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, false)
                        .with(
                                SuggestionListProperties.DRAW_OVER_ANCHOR,
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                                        && !forcePhoneStyleOmnibox)
                        .with(SuggestionListProperties.SUGGESTION_MODELS, listItems)
                        .with(SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED, true)
                        .build();

        mMediator =
                new AutocompleteMediator(
                        context,
                        delegate,
                        urlBarEditingTextProvider,
                        listModel,
                        new Handler(),
                        modalDialogManagerSupplier,
                        activityTabSupplier,
                        shareDelegateSupplier,
                        locationBarDataProvider,
                        bringTabGroupToForegroundCallback,
                        bookmarkState,
                        omniboxActionDelegate,
                        lifecycleDispatcher,
                        dropdownEmbedder,
                        windowAndroid,
                        deferredIMEWindowInsetApplicationCallback,
                        fuseboxCoordinator,
                        forcePhoneStyleOmnibox);
        mMediator.initDefaultProcessors();

        if (scrollListener != null) {
            mScrollListenerList.addObserver(scrollListener);
        }
        mScrollListenerList.addObserver(mMediator);
        listModel.set(SuggestionListProperties.GESTURE_OBSERVER, mMediator);
        listModel.set(
                SuggestionListProperties.NAVIGATION_LISTENER,
                mMediator::onSuggestionDropdownNavigation);
        listModel.set(
                SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER,
                mMediator::onSuggestionDropdownHeightChanged);
        listModel.set(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER, this::dropdownScrolled);
        listModel.set(
                SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER,
                this::dropdownOverscrolledToTop);
        listModel.set(
                SuggestionListProperties.DROPDOWN_SCROLL_OFFSET_LISTENER,
                this::dropdownScrollOffsetChanged);

        mViewProvider = new SuggestionListViewHolderProvider(listItems);
        mViewProvider.whenLoaded(
                (holder) -> {
                    mContainer = holder.container;
                    mDropdown = holder.dropdown;
                    mDropdown.setFuseboxCoordinator(fuseboxCoordinator);
                });
        LazyConstructionPropertyMcp.create(
                listModel,
                SuggestionListProperties.OMNIBOX_SESSION_ACTIVE,
                mViewProvider,
                SuggestionListViewBinder::bind);

        BaseSuggestionViewBinder.resetCachedResources();

        mProfileSupplier = profileObservableSupplier;
        mProfileChangeCallback = this::setAutocompleteProfile;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileChangeCallback);

        // When AsyncViewInflation is disabled, OmniboxSuggestionsDropdown cannot create the
        // recycled view pool b/c it causes issues with the timing of prewarming views. Creation of
        // the pool is moved to the AutocompleteCoordinator so AutocompleteCoordinator can
        // tell the pool to start prewarming and then pass it to the dropdown.
        if (!OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            mViewHolderFactory = new OmniboxViewHolderFactory();
            mRecycledViewPool = new PreWarmingRecycledViewPool(mViewHolderFactory, context);
        } else {
            mViewHolderFactory = null;
            mRecycledViewPool = null;
        }

        // https://crbug.com/41460582 Set initial layout direction ahead of inflating the
        // suggestions.
        updateSuggestionListLayoutDirection();
    }

    @VisibleForTesting
    AutocompleteCoordinator(
            ViewGroup parent,
            AutocompleteMediator mediator,
            MonotonicObservableSupplier<Profile> profileObservableSupplier,
            LocationBarEmbedder locationBarEmbedder,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier) {
        mParent = parent;
        mMediator = mediator;
        mProfileSupplier = profileObservableSupplier;
        mProfileChangeCallback = this::setAutocompleteProfile;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileChangeCallback);
        mLocationBarEmbedder = locationBarEmbedder;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mViewProvider = new SuggestionListViewHolderProvider(new ModelList());
        mViewHolderFactory = null;
        mRecycledViewPool = null;
    }

    /** Clean up resources used by this class. */
    public void destroy() {
        mProfileSupplier.removeObserver(mProfileChangeCallback);
        mMediator.destroy();
        if (mContainer != null) {
            mContainer.destroy();
            mContainer = null;
        }

        // This only occurs when AsyncViewInflation is disabled.
        if (mRecycledViewPool != null) {
            mRecycledViewPool.destroy();
        }
    }

    /**
     * Sets the observer watching the state of the omnibox suggestions. This observer will be
     * notifying of visual changes to the omnibox suggestions view, such as visibility or background
     * color changes.
     */
    @Override
    public void setOmniboxSuggestionsVisualStateObserver(
            @Nullable OmniboxSuggestionsVisualStateObserver omniboxSuggestionsVisualStateObserver) {
        mMediator.setOmniboxSuggestionsVisualStateObserver(omniboxSuggestionsVisualStateObserver);
    }

    public @Nullable OmniboxSuggestionsContainer getSuggestionsContainer() {
        if (mContainer == null) {
            mViewProvider.setForceSyncInflate(true);
            mViewProvider.inflate();
        }
        return mContainer;
    }

    class SuggestionListViewHolderProvider implements ViewProvider<SuggestionListViewHolder> {
        private final List<Callback<SuggestionListViewHolder>> mCallbacks = new ArrayList<>();
        private @Nullable SuggestionListViewHolder mHolder;
        private boolean mForceSyncInflate;
        private final ModelList mListItems;

        SuggestionListViewHolderProvider(ModelList listItems) {
            mListItems = listItems;
        }

        @Override
        @Initializer
        public void inflate() {
            AsyncViewStub stub = mLocationBarEmbedder.getSuggestionsContainerStub();
            if (stub == null) return;

            stub.setShouldInflateOnBackgroundThread(
                    !mForceSyncInflate && OmniboxFeatures.sAsyncViewInflation.isEnabled());
            @IdRes int inflatedId = mLocationBarEmbedder.getSuggestionsContainerInflatedViewId();
            AsyncViewProvider<ViewGroup> asyncProvider = AsyncViewProvider.of(stub, inflatedId);
            asyncProvider.whenLoaded(this::onAsyncInflationComplete);
            try (TimingMetric metric =
                            OmniboxMetrics.recordSuggestionsContainerInflationThreadTime();
                    TimingMetric metric2 =
                            OmniboxMetrics.recordSuggestionsContainerInflationWallTime()) {
                asyncProvider.inflate();
            }
        }

        void setForceSyncInflate(boolean forceSyncInflate) {
            mForceSyncInflate = forceSyncInflate;
        }

        private void onAsyncInflationComplete(ViewGroup container) {
            OmniboxSuggestionsContainer suggestionsContainer =
                    (OmniboxSuggestionsContainer) container;
            OmniboxSuggestionsDropdown dropdown =
                    container.findViewById(R.id.omnibox_suggestions_dropdown);

            dropdown.setModelList(mListItems);
            mHolder = new SuggestionListViewHolder(suggestionsContainer, dropdown);

            if (mRecycledViewPool != null) {
                dropdown.setRecycledViewPool(mRecycledViewPool);
            }

            for (int i = 0; i < mCallbacks.size(); i++) {
                mCallbacks.get(i).onResult(mHolder);
            }
            mCallbacks.clear();
        }

        @Override
        public void whenLoaded(Callback<SuggestionListViewHolder> callback) {
            if (mHolder != null) {
                callback.onResult(mHolder);
                return;
            }
            mCallbacks.add(callback);
        }
    }

    /**
     * Starts a new / resumes existing omnibox session.
     *
     * @param session The session state for this session. A new session may be applied without going
     *     through the endInput() (valid -> valid). This is the case for tab switching.
     */
    public void beginInput(FuseboxSessionState session) {
        if (!mDropdownAvailableRecorded && OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            mDropdownAvailableRecorded = true;
            OmniboxMetrics.recordAsyncInflationDropdownAvailable(mContainer != null);
        }

        mMediator.beginInput(session);
    }

    /** Ends the current omnibox session. */
    public void endInput() {
        mMediator.endInput();
    }

    /**
     * Serve Java-cached ZPS before session can be started with Autocomplete support.
     *
     * @param input The input to serve ZPS for.
     */
    public void serveCachedZeroSuggest(AutocompleteInput input) {
        mMediator.serveCachedZeroSuggest(input);
    }

    public void onUrlAnimationFinished() {
        mMediator.onUrlAnimationFinished();
    }

    /**
     * Setup the animation for showing the suggestions list. If the animation exists and can be
     * synchronized, it is returned in an unstarted state; otherwise null is returned.
     */
    public @Nullable Animator setupSuggestionsListShowAnimation() {
        return mMediator.setupSuggestionsListShowAnimation();
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     *
     * @param profile The profile to be used.
     */
    @VisibleForTesting
    public void setAutocompleteProfile(Profile profile) {
        mMediator.setAutocompleteProfile(profile);
    }

    /** Whether omnibox autocomplete should currently be prevented from generating suggestions. */
    public void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mMediator.setShouldPreventOmniboxAutocomplete(prevent);
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mMediator.getSuggestionCount();
    }

    /**
     * Retrieve the omnibox suggestion at the specified index. The index represents the ordering in
     * the underlying model. The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public @Nullable AutocompleteMatch getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    /** Signals that native initialization has completed. */
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
        if (OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            mViewProvider.inflate();
        }

        if (mRecycledViewPool != null) {
            mRecycledViewPool.onNativeInitialized();
        }
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    public void onVoiceResults(@Nullable List<VoiceRecognitionIntentHandler.VoiceResult> results) {
        mMediator.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results. TODO(ender): Figure out how
     *     to remove this.
     */
    public long getCurrentNativeAutocompleteResult() {
        return mMediator.getCurrentNativeAutocompleteResult();
    }

    /** Update the layout direction of the suggestion list based on the parent layout direction. */
    public void updateSuggestionListLayoutDirection() {
        mMediator.setLayoutDirection(ViewCompat.getLayoutDirection(mParent));
    }

    /**
     * Update the visuals of the autocomplete UI.
     *
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    public void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mMediator.updateVisualsForState(brandedColorScheme);
    }

    /**
     * @return Whether the coordinator has an AutocompleteController.
     */
    public boolean hasAutocompleteController() {
        return mMediator.hasAutocompleteController();
    }

    /**
     * Handle the key events associated with the suggestion list.
     *
     * @param keyCode The keycode representing what key was interacted with.
     * @param event The key event containing all meta-data associated with the event.
     * @return Whether the key event was handled.
     */
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        // Note: this method receives key events for key presses and key releases.
        // Make sure we focus only on key press events alone.
        if (!KeyNavigationUtil.isActionDown(event)) {
            return false;
        }

        boolean isShowingList = mContainer != null && mContainer.isShown();

        // Always handle <ENTER> key, even if the suggestions list is not showing.
        // This allows users to navigate to the typed url or query.
        // Try to dispatch to suggestions list, if one is showing, otherwise invoke navigation.
        if (KeyNavigationUtil.isEnter(event)) {
            if (isShowingList && assumeNonNull(mContainer).onKeyDown(keyCode, event)) {
                return true;
            }

            boolean openInNewTab = event.isAltPressed();
            boolean openInNewWindow = !openInNewTab && event.isShiftPressed();
            if (mParent.getVisibility() == View.VISIBLE && mMediator.hasAutocompleteController()) {
                mMediator.loadTypedOmniboxText(event.getEventTime(), openInNewTab, openInNewWindow);
                return true;
            }

            return false;
        }

        // Do not attempt to interpret any navigation keys when the suggestions list is not showing.
        if (!isShowingList) {
            return false;
        }

        // Do not attempt to interpret non-navigaton keys.
        // There are cases where the SPACE key may gen inappropriately routed to the
        // Suggestion, simulating press/long press of the UI element.
        if ((keyCode == KeyEvent.KEYCODE_DPAD_UP)
                || (keyCode == KeyEvent.KEYCODE_DPAD_DOWN)
                || KeyNavigationUtil.isTabNavigation(event)) {
            mMediator.allowPendingItemSelection();
            assumeNonNull(mContainer).onKeyDown(keyCode, event);
            return true;
        }

        return false;
    }

    /** Site search was successfully triggered. */
    public boolean triggerSiteSearch(@SiteSearchActivationSource int source) {
        return mMediator.triggerSiteSearch(source);
    }

    /** Notify the Autocomplete about Omnibox text change. */
    public void onInputChanged() {
        mMediator.onInputChanged();
    }

    /** Trigger autocomplete for the given query. */
    public void startAutocompleteForQuery(String query) {
        mMediator.startAutocompleteForQuery(query);
    }

    /**
     * Given a search query, this will attempt to see if the query appears to be portion of a
     * properly formed URL. If it appears to be a URL, this will return the fully qualified version
     * (i.e. including the scheme, etc...). If the query does not appear to be a URL, this will
     * return null.
     *
     * <p>Note:
     *
     * <ul>
     *   <li>This call is VERY expensive. Use only when it is absolutely necessary to get the exact
     *       information about how a given query string will be interpreted. For less restrictive
     *       URL vs text matching, please defer to GURL.
     *   <li>This updates the internal state of the autocomplete controller just as start() does.
     *       Future calls that reference autocomplete results by index, e.g. onSuggestionSelected(),
     *       should reference the returned suggestion by index 0.
     * </ul>
     *
     * @param profile The profile to expand the query for.
     * @param query The query to be expanded into a fully qualified URL if appropriate.
     * @return The AutocompleteMatch for a default / top match. This may be either SEARCH match
     *     built with the user's default search engine, or a NAVIGATION match.
     */
    public static @Nullable AutocompleteMatch classify(Profile profile, String query) {
        var controller = AutocompleteController.getForProfile(profile);
        return (controller != null) ? controller.classify(query) : null;
    }

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     *
     * @param tab The current tab.
     */
    public void prefetchZeroSuggestResults(@Nullable Tab tab) {
        mMediator.startPrefetch(tab != null ? tab.getWebContents() : null);
    }

    /** Stop current suggestions requests and clear the suggestions list. */
    public void stopAutocomplete() {
        mMediator.stopAutocomplete(AutocompleteStopReason.CLOBBERED);
    }

    /** {@see AutocompleteMediator#loadUrlFromVoice(String)} */
    public void loadUrlFromVoice(String query) {
        mMediator.loadUrlFromVoice(query);
    }

    /** Returns whether Autocomplete is serving suggestions. */
    public boolean isServingSuggestions() {
        return mMediator.isInInputSession()
                && mContainer != null
                && mContainer.isShown()
                && mMediator.getSuggestionCount() > 0;
    }

    /**
     * @return Suggestions Dropdown view, showing the list of suggestions.
     */
    public @Nullable View getSuggestionsDropdown() {
        return mDropdown;
    }

    /**
     * @return Suggestions Dropdown view, showing the list of suggestions.
     */
    public @Nullable OmniboxSuggestionsContainer getSuggestionsContainerForTest() {
        return mContainer;
    }

    public void setSuggestionsContainerForTest(OmniboxSuggestionsContainer container) {
        OmniboxSuggestionsContainer oldValue = mContainer;
        mContainer = container;
        ResettersForTesting.register(() -> mContainer = oldValue);
    }

    /**
     * @return The current receiving OnSuggestionsReceived events.
     */
    public OnSuggestionsReceivedListener getSuggestionsReceivedListenerForTest() {
        return mMediator;
    }

    /**
     * @return The ModelList for the currently shown suggestions.
     */
    public ModelList getSuggestionModelListForTest() {
        return mMediator.getSuggestionModelListForTest();
    }

    public @Nullable ModalDialogManager getModalDialogManagerForTest() {
        return mModalDialogManagerSupplier.get();
    }

    public void stopAutocompleteForTest(@AutocompleteStopReason int stopReason) {
        mMediator.stopAutocomplete(stopReason);
    }

    /**
     * Notify the {@link OmniboxSuggestionsDropdownScrollListener} that the dropdown is scrolled.
     */
    public void dropdownScrolled() {
        for (OmniboxSuggestionsDropdownScrollListener listener : mScrollListenerList) {
            listener.onSuggestionDropdownScroll();
        }
    }

    /**
     * Notify the {@link OmniboxSuggestionsDropdownScrollListener} that the dropdown is scrolled to
     * the top.
     */
    public void dropdownOverscrolledToTop() {
        for (OmniboxSuggestionsDropdownScrollListener listener : mScrollListenerList) {
            listener.onSuggestionDropdownOverscrolledToTop();
        }
    }

    public void dropdownScrollOffsetChanged(int newOffset) {
        for (OmniboxSuggestionsDropdownScrollListener listener : mScrollListenerList) {
            listener.onSuggestionDropdownScrollOffsetChanged(newOffset);
        }
    }

    /** Adds an observer for suggestions scroll events. */
    public void addOmniboxSuggestionsDropdownScrollListener(
            OmniboxSuggestionsDropdownScrollListener listener) {
        mScrollListenerList.addObserver(listener);
    }

    /** Removes an observer for suggestions scroll events. */
    public void removeOmniboxSuggestionsDropdownScrollListener(
            OmniboxSuggestionsDropdownScrollListener listener) {
        mScrollListenerList.removeObserver(listener);
    }
}
