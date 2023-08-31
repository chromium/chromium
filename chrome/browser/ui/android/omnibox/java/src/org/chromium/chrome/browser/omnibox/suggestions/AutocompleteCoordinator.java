// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.dividerline.DividerLineView;
import org.chromium.chrome.browser.omnibox.suggestions.dividerline.DividerLineViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.history_clusters.HistoryClustersProcessor.OpenHistoryClustersDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that handles the interactions with the autocomplete system.
 */
public class AutocompleteCoordinator implements UrlFocusChangeListener, UrlTextChangeListener {
    private final @NonNull ViewGroup mParent;
    private final @NonNull ObservableSupplier<Profile> mProfileSupplier;
    private final @NonNull Callback<Profile> mProfileChangeCallback;
    private final @NonNull AutocompleteMediator mMediator;
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private @Nullable OmniboxSuggestionsDropdown mDropdown;
    private @NonNull ObserverList<OmniboxSuggestionsDropdownScrollListener> mScrollListenerList =
            new ObserverList<>();
    private OmniboxSuggestionsDropdownAdapter mAdapter;
    private Context mContext;
    private boolean mUrlHasFocus;
    private OneshotSupplierImpl<OmniboxSuggestionsDropdownAdapter> mAdapterSupplier =
            new OneshotSupplierImpl<>();
    private PreWarmingRecycledViewPool mRecycledViewPool;

    public AutocompleteCoordinator(@NonNull ViewGroup parent,
            @NonNull AutocompleteControllerProvider controllerProvider,
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
            @NonNull OmniboxSuggestionsDropdownScrollListener scrollListener,
            @NonNull OpenHistoryClustersDelegate openHistoryClustersDelegate) {
        mParent = parent;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        Context context = parent.getContext();
        mContext = context;

        PropertyModel listModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        ModelList listItems = new ModelList();

        listModel.set(SuggestionListProperties.EMBEDDER, dropdownEmbedder);
        listModel.set(SuggestionListProperties.VISIBLE, false);
        listModel.set(SuggestionListProperties.SUGGESTION_MODELS, listItems);

        mMediator = new AutocompleteMediator(context, controllerProvider, delegate,
                urlBarEditingTextProvider, listModel, new Handler(), modalDialogManagerSupplier,
                activityTabSupplier, shareDelegateSupplier, locationBarDataProvider,
                bringToForegroundCallback, tabWindowManagerSupplier, bookmarkState,
                omniboxActionDelegate, openHistoryClustersDelegate);
        mMediator.initDefaultProcessors();

        mScrollListenerList.addObserver(scrollListener);
        mScrollListenerList.addObserver(mMediator);
        listModel.set(SuggestionListProperties.GESTURE_OBSERVER, mMediator);
        listModel.set(SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER,
                mMediator::onSuggestionDropdownHeightChanged);
        listModel.set(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER, this::dropdownScrolled);
        listModel.set(SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER,
                this::dropdownOverscrolledToTop);

        ViewProvider<SuggestionListViewHolder> viewProvider =
                createViewProvider(context, listItems);
        viewProvider.whenLoaded((holder) -> { mDropdown = holder.dropdown; });
        LazyConstructionPropertyMcp.create(listModel, SuggestionListProperties.VISIBLE,
                viewProvider, SuggestionListViewBinder::bind);

        mProfileSupplier = profileObservableSupplier;
        mProfileChangeCallback = this::setAutocompleteProfile;
        mProfileSupplier.addObserver(mProfileChangeCallback);

        mAdapter = createAdapter(listItems);
        mRecycledViewPool = new PreWarmingRecycledViewPool(mAdapter, context, new Handler());

        // https://crbug.com/966227 Set initial layout direction ahead of inflating the suggestions.
        updateSuggestionListLayoutDirection();
    }

    /**
     * Clean up resources used by this class.
     */
    public void destroy() {
        mRecycledViewPool.destroy();
        mProfileSupplier.removeObserver(mProfileChangeCallback);
        mMediator.destroy();
        if (mDropdown != null) {
            mDropdown.destroy();
            mDropdown = null;
        }
    }

    private ViewProvider<SuggestionListViewHolder> createViewProvider(
            Context context, MVCListAdapter.ModelList modelList) {
        return new ViewProvider<SuggestionListViewHolder>() {
            private List<Callback<SuggestionListViewHolder>> mCallbacks = new ArrayList<>();
            private SuggestionListViewHolder mHolder;

            @Override
            public void inflate() {
                OmniboxSuggestionsDropdown dropdown;
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    dropdown = new OmniboxSuggestionsDropdown(context, mRecycledViewPool);
                }

                dropdown.getViewGroup().setClipToPadding(false);
                dropdown.setAdapter(mAdapter);

                ViewGroup container = (ViewGroup) ((ViewStub) mParent.getRootView().findViewById(
                                                           R.id.omnibox_results_container_stub))
                                              .inflate();

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

    private OmniboxSuggestionsDropdownAdapter createAdapter(ModelList listItems) {
        BaseSuggestionViewBinder.resetCachedDimensions();
        OmniboxSuggestionsDropdownAdapter adapter =
                new OmniboxSuggestionsDropdownAdapter(listItems);

        // Note: clang-format does a bad job formatting lambdas so we turn it off here.
        // clang-format off
        // Register a view type for a default omnibox suggestion.
        adapter.registerType(
            OmniboxSuggestionUiType.DEFAULT,
            parent -> new BaseSuggestionView<View>(
                parent.getContext(), R.layout.omnibox_basic_suggestion),
            new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

        adapter.registerType(
            OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
            parent -> new EditUrlSuggestionView(parent.getContext()),
            new EditUrlSuggestionViewBinder());

        adapter.registerType(
            OmniboxSuggestionUiType.ANSWER_SUGGESTION,
            parent -> new BaseSuggestionView<View>(
                parent.getContext(), R.layout.omnibox_answer_suggestion),
            new BaseSuggestionViewBinder<View>(AnswerSuggestionViewBinder::bind));

        adapter.registerType(
            OmniboxSuggestionUiType.ENTITY_SUGGESTION,
            parent -> new BaseSuggestionView<View>(
                parent.getContext(), R.layout.omnibox_basic_suggestion),
            new BaseSuggestionViewBinder<View>(EntitySuggestionViewBinder::bind));

        adapter.registerType(
            OmniboxSuggestionUiType.TAIL_SUGGESTION,
            parent -> new BaseSuggestionView<TailSuggestionView>(
                new TailSuggestionView(parent.getContext())),
            new BaseSuggestionViewBinder<TailSuggestionView>(
                TailSuggestionViewBinder::bind));

        adapter.registerType(
            OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
            parent -> new BaseSuggestionView<View>(
                parent.getContext(), R.layout.omnibox_basic_suggestion),
            new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

        adapter.registerType(
            OmniboxSuggestionUiType.TILE_NAVSUGGEST,
            BaseCarouselSuggestionItemViewBuilder::createView,
            BaseCarouselSuggestionViewBinder::bind);

        adapter.registerType(
            OmniboxSuggestionUiType.HEADER,
            parent -> new HeaderView(parent.getContext()),
            HeaderViewBinder::bind);

        adapter.registerType(
            OmniboxSuggestionUiType.PEDAL_SUGGESTION,
            parent -> new BaseSuggestionView<View>(
                parent.getContext(), R.layout.omnibox_basic_suggestion),
            new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

        adapter.registerType(
            OmniboxSuggestionUiType.DIVIDER_LINE,
            parent -> new DividerLineView(parent.getContext()),
            DividerLineViewBinder::bind);
        // clang-format on
        return adapter;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
        mMediator.onOmniboxSessionStateChange(hasFocus);
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        mMediator.onUrlAnimationFinished(hasFocus);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void setAutocompleteProfile(Profile profile) {
        mMediator.setAutocompleteProfile(profile);
    }

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
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
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public AutocompleteMatch getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
        mRecycledViewPool.onNativeInitialized();
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    public void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results) {
        mMediator.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     * TODO(ender): Figure out how to remove this.
     */
    public long getCurrentNativeAutocompleteResult() {
        return mMediator.getCurrentNativeAutocompleteResult();
    }

    /**
     * Update the layout direction of the suggestion list based on the parent layout direction.
     */
    public void updateSuggestionListLayoutDirection() {
        mMediator.setLayoutDirection(ViewCompat.getLayoutDirection(mParent));
    }

    /**
     * Update the visuals of the autocomplete UI.
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    public void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mMediator.updateVisualsForState(brandedColorScheme);
    }

    /**
     * Show cached zero suggest results.
     * Enables Autocomplete subsystem to offer most recently presented suggestions in the event
     * where Native counterpart is not yet initialized.
     */
    public void startCachedZeroSuggest() {
        mMediator.startCachedZeroSuggest();
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
        boolean isAnyDirection = KeyNavigationUtil.isGoAnyDirection(event);

        if (isShowingList && mMediator.getSuggestionCount() > 0 && isAnyDirection) {
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

    @Override
    public void onTextChanged(String textWithoutAutocomplete) {
        mMediator.onTextChanged(textWithoutAutocomplete);
    }

    /**
     * Trigger autocomplete for the given query.
     */
    public void startAutocompleteForQuery(String query) {
        mMediator.startAutocompleteForQuery(query);
    }

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     */
    public void prefetchZeroSuggestResults() {
        mMediator.startPrefetch();
    }

    /** @return Suggestions Dropdown view, showing the list of suggestions. */
    public OmniboxSuggestionsDropdown getSuggestionsDropdownForTest() {
        return mDropdown;
    }

    /** @return The current receiving OnSuggestionsReceived events. */
    public OnSuggestionsReceivedListener getSuggestionsReceivedListenerForTest() {
        return mMediator;
    }

    /** @return The ModelList for the currently shown suggestions. */
    public ModelList getSuggestionModelListForTest() {
        return mMediator.getSuggestionModelListForTest();
    }

    public @NonNull ModalDialogManager getModalDialogManagerForTest() {
        assert mModalDialogManagerSupplier.hasValue();
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
}
