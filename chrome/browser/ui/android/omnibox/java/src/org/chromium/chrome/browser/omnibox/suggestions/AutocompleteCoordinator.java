// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Coordinator that handles the interactions with the autocomplete system. */
public class AutocompleteCoordinator
        implements UrlFocusChangeListener, OmniboxSuggestionsVisualState {
    private final @NonNull ViewGroup mParent;
    private final @NonNull ObservableSupplier<Profile> mProfileSupplier;
    private final @NonNull Callback<Profile> mProfileChangeCallback;
    private final @NonNull AutocompleteMediator mMediator;
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final @NonNull OmniboxSuggestionsDropdownAdapter mAdapter;
    private final @NonNull Optional<PreWarmingRecycledViewPool> mRecycledViewPool;
    private @Nullable OmniboxSuggestionsDropdown mDropdown;
    private @NonNull ObserverList<OmniboxSuggestionsDropdownScrollListener> mScrollListenerList =
            new ObserverList<>();

    /** An observer watching for changes to the visual state of the omnibox suggestions. */
    public interface OmniboxSuggestionsVisualStateObserver {
        /** Called when the Omnibox session state changes. */
        void onOmniboxSessionStateChange(boolean isActive);

        /** Called when the background color of the omnibox suggestions changes. */
        void onOmniboxSuggestionsBackgroundColorChanged(@ColorInt int color);
    }

    public AutocompleteCoordinator(
            @NonNull ViewGroup parent,
            @NonNull AutocompleteDelegate delegate,
            @NonNull OmniboxSuggestionsDropdownEmbedder dropdownEmbedder,
            @NonNull UrlBarEditingTextStateProvider urlBarEditingTextProvider,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Supplier<Tab> activityTabSupplier,
            @Nullable Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull ObservableSupplier<Profile> profileObservableSupplier,
            @NonNull Callback<Tab> bringToForegroundCallback,
            @NonNull Supplier<TabWindowManager> tabWindowManagerSupplier,
            @NonNull BookmarkState bookmarkState,
            @NonNull OmniboxActionDelegate omniboxActionDelegate,
            @Nullable OmniboxSuggestionsDropdownScrollListener scrollListener,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            boolean forcePhoneStyleOmnibox,
            @NonNull WindowAndroid windowAndroid,
            @NonNull
                    DeferredIMEWindowInsetApplicationCallback
                            deferredIMEWindowInsetApplicationCallback) {
        mParent = parent;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        Context context = parent.getContext();

        ModelList listItems = new ModelList();
        PropertyModel listModel =
                new PropertyModel.Builder(SuggestionListProperties.ALL_KEYS)
                        .with(SuggestionListProperties.EMBEDDER, dropdownEmbedder)
                        .with(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, false)
                        .with(SuggestionListProperties.DRAW_OVER_ANCHOR, false)
                        .with(SuggestionListProperties.SUGGESTION_MODELS, listItems)
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
                        bringToForegroundCallback,
                        tabWindowManagerSupplier,
                        bookmarkState,
                        omniboxActionDelegate,
                        lifecycleDispatcher,
                        dropdownEmbedder,
                        windowAndroid,
                        deferredIMEWindowInsetApplicationCallback);
        mMediator.initDefaultProcessors();

        if (scrollListener != null) {
            mScrollListenerList.addObserver(scrollListener);
        }
        mScrollListenerList.addObserver(mMediator);
        listModel.set(SuggestionListProperties.GESTURE_OBSERVER, mMediator);
        listModel.set(
                SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER,
                mMediator::onSuggestionDropdownHeightChanged);
        listModel.set(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER, this::dropdownScrolled);
        listModel.set(
                SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER,
                this::dropdownOverscrolledToTop);

        ViewProvider<SuggestionListViewHolder> viewProvider =
                createViewProvider(context, forcePhoneStyleOmnibox);
        viewProvider.whenLoaded(
                (holder) -> {
                    mDropdown = holder.dropdown;
                });
        LazyConstructionPropertyMcp.create(
                listModel,
                SuggestionListProperties.OMNIBOX_SESSION_ACTIVE,
                viewProvider,
                SuggestionListViewBinder::bind);

        BaseSuggestionViewBinder.resetCachedResources();

        mProfileSupplier = profileObservableSupplier;
        mProfileChangeCallback = this::setAutocompleteProfile;
        mProfileSupplier.addObserver(mProfileChangeCallback);
        mAdapter = new OmniboxSuggestionsDropdownAdapter(listItems);

        if (!OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            mRecycledViewPool = Optional.of(new PreWarmingRecycledViewPool(mAdapter, context));
        } else {
            mRecycledViewPool = Optional.empty();
        }

        // https://crbug.com/966227 Set initial layout direction ahead of inflating the suggestions.
        updateSuggestionListLayoutDirection();
    }

    /** Clean up resources used by this class. */
    public void destroy() {
        mRecycledViewPool.ifPresent(p -> p.destroy());
        mProfileSupplier.removeObserver(mProfileChangeCallback);
        mMediator.destroy();
        if (mDropdown != null) {
            mDropdown.destroy();
            mDropdown = null;
        }
    }

    /**
     * Sets the observer watching the state of the omnibox suggestions. This observer will be
     * notifying of visual changes to the omnibox suggestions view, such as visibility or background
     * color changes.
     */
    @Override
    public void setOmniboxSuggestionsVisualStateObserver(
            Optional<OmniboxSuggestionsVisualStateObserver> omniboxSuggestionsVisualStateObserver) {
        mMediator.setOmniboxSuggestionsVisualStateObserver(omniboxSuggestionsVisualStateObserver);
    }

    private ViewProvider<SuggestionListViewHolder> createViewProvider(
            Context context, boolean forcePhoneStyleOmnibox) {
        return new ViewProvider<SuggestionListViewHolder>() {
            private AsyncViewProvider<ViewGroup> mAsyncProvider;
            private List<Callback<SuggestionListViewHolder>> mCallbacks = new ArrayList<>();
            private SuggestionListViewHolder mHolder;

            @Override
            public void inflate() {
                AsyncViewStub stub =
                        mParent.getRootView().findViewById(R.id.omnibox_results_container_stub);
                stub.setShouldInflateOnBackgroundThread(
                        OmniboxFeatures.sAsyncViewInflation.isEnabled());
                mAsyncProvider = AsyncViewProvider.of(stub, R.id.omnibox_results_container);
                mAsyncProvider.whenLoaded(this::onAsyncInflationComplete);
                mAsyncProvider.inflate();
            }

            private void onAsyncInflationComplete(ViewGroup container) {
                OmniboxSuggestionsDropdown dropdown =
                        container.findViewById(R.id.omnibox_suggestions_dropdown);

                dropdown.forcePhoneStyleOmnibox(forcePhoneStyleOmnibox);
                dropdown.setAdapter(mAdapter);
                mRecycledViewPool.ifPresent(p -> dropdown.setRecycledViewPool(p));
                mHolder = new SuggestionListViewHolder(container, dropdown);
                for (int i = 0; i < mCallbacks.size(); i++) {
                    mCallbacks.get(i).onResult(mHolder);
                }
                mCallbacks = null;
            }

            @Override
            public void whenLoaded(Callback<SuggestionListViewHolder> callback) {
                if (mHolder != null) {
                    callback.onResult(mHolder);
                    return;
                }
                mCallbacks.add(callback);
            }
        };
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mMediator.onOmniboxSessionStateChange(hasFocus);
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        mMediator.onUrlAnimationFinished(hasFocus);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     *
     * @param profile The profile to be used.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
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
    public AutocompleteMatch getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    /** Signals that native initialization has completed. */
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
        mRecycledViewPool.ifPresent(p -> p.onNativeInitialized());
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    public void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results) {
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

        boolean isShowingList = mDropdown != null && mDropdown.getViewGroup().isShown();

        // List of keys used to navigate the suggestions list.
        boolean isSelectionKey =
                (keyCode == KeyEvent.KEYCODE_DPAD_UP)
                        || (keyCode == KeyEvent.KEYCODE_DPAD_DOWN)
                        || (keyCode == KeyEvent.KEYCODE_TAB);

        if (isShowingList && event.getKeyCode() == KeyEvent.KEYCODE_ESCAPE) {
            mMediator.finishInteraction();
            return true;
        }
        if (isShowingList && isSelectionKey) {
            mMediator.allowPendingItemSelection();
        }
        if (isShowingList && mDropdown.getViewGroup().onKeyDown(keyCode, event)) {
            return true;
        }
        if (KeyNavigationUtil.isEnter(event) && mParent.getVisibility() == View.VISIBLE) {
            mMediator.loadTypedOmniboxText(event.getEventTime(), event.isAltPressed());
            return true;
        }
        return false;
    }

    /** Notify the Autocomplete about Omnibox text change. */
    public void onTextChanged(String textWithoutAutocomplete) {
        mMediator.onTextChanged(textWithoutAutocomplete, /* isOnFocusContext= */ false);
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
     *     built with the user's default search engine, or a NAVIGATION match. The call might return
     *     null if it is invoked before Native libraries are initialized, or if the Profile is
     *     invalid.
     */
    public static @Nullable AutocompleteMatch classify(
            @NonNull Profile profile, @NonNull String query) {
        return AutocompleteController.getForProfile(profile)
                .map(a -> a.classify(query))
                .orElse(null);
    }

    /** Sends a zero suggest request to the server in order to pre-populate the result cache. */
    public void prefetchZeroSuggestResults() {
        mMediator.startPrefetch();
    }

    /**
     * @return Suggestions Dropdown view, showing the list of suggestions.
     */
    public OmniboxSuggestionsDropdown getSuggestionsDropdownForTest() {
        return mDropdown;
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

    public @NonNull ModalDialogManager getModalDialogManagerForTest() {
        return mModalDialogManagerSupplier.get();
    }

    public void stopAutocompleteForTest(boolean clearResults) {
        mMediator.stopAutocomplete(clearResults);
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
