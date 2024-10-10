// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.view.Window;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics.RefineActionUsage;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionFactoryImpl;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxAnswerAction;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;

/** Handles updating the model state for the currently visible omnibox suggestions. */
class AutocompleteMediator
        implements OnSuggestionsReceivedListener,
                OmniboxSuggestionsDropdown.GestureObserver,
                OmniboxSuggestionsDropdownScrollListener,
                TopResumedActivityChangedObserver,
                SuggestionHost {
    private static final int SCHEDULE_FOR_IMMEDIATE_EXECUTION = -1;

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;

    private final @NonNull Context mContext;
    private final @NonNull AutocompleteDelegate mDelegate;
    private final @NonNull UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final @NonNull PropertyModel mListPropertyModel;
    private final @NonNull ModelList mSuggestionModels;
    private final @NonNull Handler mHandler;
    private final @NonNull LocationBarDataProvider mDataProvider;
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final @NonNull DropdownItemViewInfoListBuilder mDropdownViewInfoListBuilder;
    private final @NonNull DropdownItemViewInfoListManager mDropdownViewInfoListManager;
    private final @NonNull Callback<Tab> mBringTabToFrontCallback;
    private final @NonNull Supplier<TabWindowManager> mTabWindowManagerSupplier;
    private final @NonNull OmniboxActionDelegate mOmniboxActionDelegate;
    private final @NonNull ActivityLifecycleDispatcher mLifecycleDispatcher;
    private @Nullable SuggestionsListAnimationDriver mAnimationDriver;
    private final @NonNull WindowAndroid mWindowAndroid;
    private final @NonNull DeferredIMEWindowInsetApplicationCallback
            mDeferredIMEWindowInsetApplicationCallback;
    private final @NonNull OmniboxSuggestionsDropdownEmbedder mEmbedder;

    private @NonNull Optional<AutocompleteController> mAutocomplete = Optional.empty();
    private @NonNull Optional<AutocompleteResult> mAutocompleteResult = Optional.empty();
    private @NonNull Optional<Runnable> mCurrentAutocompleteRequest = Optional.empty();
    private @NonNull Optional<Runnable> mDeferredLoadAction = Optional.empty();
    private @NonNull Optional<PropertyModel> mDeleteDialogModel = Optional.empty();

    private boolean mNativeInitialized;
    private boolean mIsInZeroPrefixContext;
    private long mUrlFocusTime;
    // When set, indicates an active omnibox session.
    private boolean mIsActive;
    // When set, specifies the system time of the most recent suggestion list request.
    private Long mLastSuggestionRequestTime;
    // When set, specifies the time when the suggestion list was shown the first time.
    // Suggestions are refreshed several times per keystroke.
    private Long mFirstSuggestionListModelCreatedTime;
    private OptionalInt mPageClassification = OptionalInt.empty();

    @IntDef({
        EditSessionState.INACTIVE,
        EditSessionState.ACTIVATED_BY_USER_INPUT,
        EditSessionState.ACTIVATED_BY_QUERY_TILE
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @interface EditSessionState {
        int INACTIVE = 0; // Omnibox is not being edited.
        int ACTIVATED_BY_USER_INPUT = 1; // The edit session is triggered by user input.
        int ACTIVATED_BY_QUERY_TILE = 2; // The edit session is triggered from query tile.
    }

    private @EditSessionState int mEditSessionState = EditSessionState.INACTIVE;

    private @RefineActionUsage int mRefineActionUsage = RefineActionUsage.NOT_USED;

    // The timestamp (using SystemClock.elapsedRealtime()) at the point when the user started
    // modifying the omnibox with new input.
    private long mNewOmniboxEditSessionTimestamp = -1;
    // Set at the end of the Omnibox interaction to indicate whether the user selected an item
    // from the list (true) or left the Omnibox and suggestions list with no action taken (false).
    private boolean mOmniboxFocusResultedInNavigation;
    // Facilitate detection of Autocomplete actions being scheduled from an Autocomplete action.
    private boolean mIsExecutingAutocompleteAction;
    // Whether user scrolled the suggestions list.
    private boolean mSuggestionsListScrolled;

    /**
     * The text shown in the URL bar (user text + inline autocomplete) after the most recent set of
     * omnibox suggestions was received. When the user presses enter in the omnibox, this value is
     * compared to the URL bar text to determine whether the first suggestion is still valid.
     */
    private String mUrlTextAfterSuggestionsReceived;

    private boolean mShouldPreventOmniboxAutocomplete;
    private long mLastActionUpTimestamp;
    private boolean mIgnoreOmniboxItemSelection = true;

    // The number of touch down events sent to native during an omnibox session.
    private int mNumTouchDownEventForwardedInOmniboxSession;
    // The number of prefetches that were started from touch down events during an omnibox session.
    private int mNumPrefetchesStartedInOmniboxSession;
    // The suggestion that the last prefetch was started for within the current omnibox session.
    private @NonNull Optional<AutocompleteMatch> mLastPrefetchStartedSuggestion = Optional.empty();

    // Observer watching for changes to the visual state of the omnibox suggestions.
    private @NonNull Optional<AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver>
            mOmniboxSuggestionsVisualStateObserver = Optional.empty();

    public AutocompleteMediator(
            @NonNull Context context,
            @NonNull AutocompleteDelegate delegate,
            @NonNull UrlBarEditingTextStateProvider textProvider,
            @NonNull PropertyModel listPropertyModel,
            @NonNull Handler handler,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Supplier<Tab> activityTabSupplier,
            @Nullable Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull Callback<Tab> bringTabToFrontCallback,
            @NonNull Supplier<TabWindowManager> tabWindowManagerSupplier,
            @NonNull BookmarkState bookmarkState,
            @NonNull OmniboxActionDelegate omniboxActionDelegate,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull OmniboxSuggestionsDropdownEmbedder embedder,
            @NonNull WindowAndroid windowAndroid,
            @NonNull
                    DeferredIMEWindowInsetApplicationCallback
                            deferredIMEWindowInsetApplicationCallback) {
        mContext = context;
        mDelegate = delegate;
        mUrlBarEditingTextProvider = textProvider;
        mListPropertyModel = listPropertyModel;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mHandler = handler;
        mDataProvider = locationBarDataProvider;
        mBringTabToFrontCallback = bringTabToFrontCallback;
        mTabWindowManagerSupplier = tabWindowManagerSupplier;
        mSuggestionModels = mListPropertyModel.get(SuggestionListProperties.SUGGESTION_MODELS);
        mOmniboxActionDelegate = omniboxActionDelegate;
        mWindowAndroid = windowAndroid;
        mEmbedder = embedder;
        mDropdownViewInfoListBuilder =
                new DropdownItemViewInfoListBuilder(activityTabSupplier, bookmarkState);
        mDropdownViewInfoListBuilder.setShareDelegateSupplier(shareDelegateSupplier);
        mDropdownViewInfoListManager =
                new DropdownItemViewInfoListManager(mSuggestionModels, context);
        OmniboxResourceProvider.invalidateDrawableCache();
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mDeferredIMEWindowInsetApplicationCallback = deferredIMEWindowInsetApplicationCallback;

        var pm = context.getPackageManager();
        var dialIntent = new Intent(Intent.ACTION_DIAL);
        OmniboxActionFactoryImpl.get()
                .setDialerAvailable(!pm.queryIntentActivities(dialIntent, 0).isEmpty());
        mListPropertyModel.set(
                SuggestionListProperties.DRAW_OVER_ANCHOR,
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && OmniboxFeatures.shouldAnimateSuggestionsListAppearance()) {
            initializeAnimationDriver(mWindowAndroid.getWindow());
        }
    }

    /**
     * Sets the observer watching the state of the omnibox suggestions. This observer will be
     * notifying of visual changes to the omnibox suggestions view, such as visibility or background
     * color changes.
     */
    void setOmniboxSuggestionsVisualStateObserver(
            Optional<AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver>
                    omniboxSuggestionsVisualStateObserver) {
        assert omniboxSuggestionsVisualStateObserver != null;
        mOmniboxSuggestionsVisualStateObserver = omniboxSuggestionsVisualStateObserver;
    }

    /** Initialize the Mediator with default set of suggestion processors. */
    void initDefaultProcessors() {
        mDropdownViewInfoListBuilder.initDefaultProcessors(
                mContext, this, mUrlBarEditingTextProvider);
    }

    /**
     * @return DropdownItemViewInfoListBuilder instance used to convert OmniboxSuggestions to list
     *     of ViewInfos.
     */
    DropdownItemViewInfoListBuilder getDropdownItemViewInfoListBuilderForTest() {
        return mDropdownViewInfoListBuilder;
    }

    public void destroy() {
        stopAutocomplete(false);
        mAutocomplete.ifPresent(a -> a.removeOnSuggestionsReceivedListener(this));

        if (mNativeInitialized) {
            OmniboxActionFactoryImpl.get().destroyNativeFactory();
        }
        mHandler.removeCallbacks(null);
        mDropdownViewInfoListBuilder.destroy();
        mLifecycleDispatcher.unregister(this);
    }

    /**
     * @return The ModelList for currently shown suggestions.
     */
    ModelList getSuggestionModelListForTest() {
        return mSuggestionModels;
    }

    /**
     * Check if the suggestion is created from clipboard.
     *
     * @param suggestion The AutocompleteMatch to check.
     * @return Whether or not the suggestion is from clipboard.
     */
    private boolean isSuggestionFromClipboard(AutocompleteMatch suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE;
    }

    /**
     * Check if the suggestion is a link created from clipboard. It can be either a suggested
     * clipboard URL, or pasting an URL into the omnibox.
     *
     * @param suggestion The AutocompleteMatch to check.
     * @return Whether or not the suggestion is a link from clipboard.
     */
    private boolean isSuggestionLinkFromClipboard(AutocompleteMatch suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || (suggestion.getType() == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                        && mUrlBarEditingTextProvider.wasLastEditPaste());
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mAutocompleteResult.map(r -> r.getSuggestionsList().size()).orElse(0);
    }

    /**
     * Retrieve the omnibox suggestion at the specified index. The index represents the ordering in
     * the underlying model. The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param matchIndex The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public AutocompleteMatch getSuggestionAt(int matchIndex) {
        return mAutocompleteResult.map(r -> r.getSuggestionsList().get(matchIndex)).orElse(null);
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     *
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        mDropdownViewInfoListManager.setLayoutDirection(layoutDirection);
    }

    /**
     * Specifies the visual state to be used by the suggestions.
     *
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mDropdownViewInfoListManager.setBrandedColorScheme(brandedColorScheme);
        mListPropertyModel.set(SuggestionListProperties.COLOR_SCHEME, brandedColorScheme);
        mOmniboxSuggestionsVisualStateObserver.ifPresent(
                (observer) ->
                        observer.onOmniboxSuggestionsBackgroundColorChanged(
                                OmniboxResourceProvider
                                        .getSuggestionsDropdownBackgroundColorForColorScheme(
                                                mContext, brandedColorScheme)));
    }

    /**
     * Show cached zero suggest results. Enables Autocomplete subsystem to offer most recently
     * presented suggestions in the event where Native counterpart is not yet initialized.
     *
     * <p>Note: the only supported page context right now is the ANDROID_SEARCH_WIDGET.
     *
     * @param isOnFocusContext Whether the request is made on focus (as opposed to on a text
     *     change).
     */
    void startCachedZeroSuggest(boolean isOnFocusContext) {
        maybeServeCachedResult();
        postAutocompleteRequest(
                () -> startZeroSuggest(isOnFocusContext), SCHEDULE_FOR_IMMEDIATE_EXECUTION);
    }

    private void maybeCacheResult(@NonNull AutocompleteResult result) {
        if (mIsInZeroPrefixContext
                && !result.isFromCachedResult()
                && mDataProvider.getPageClassification(false)
                        == PageClassification.ANDROID_SEARCH_WIDGET_VALUE) {
            CachedZeroSuggestionsManager.saveToCache(result);
        }
    }

    private void maybeServeCachedResult() {
        int pageClass = mDataProvider.getPageClassification(false);
        if (mAutocomplete.isPresent()
                || !mIsInZeroPrefixContext
                || (pageClass != PageClassification.ANDROID_SEARCH_WIDGET_VALUE
                        && pageClass != PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE)) {
            return;
        }
        onSuggestionsReceived(CachedZeroSuggestionsManager.readFromCache(), true);
    }

    /** Notify the mediator that a item selection is pending and should be accepted. */
    void allowPendingItemSelection() {
        mIgnoreOmniboxItemSelection = false;
    }

    /** Signals that native initialization has completed. */
    void onNativeInitialized() {
        mNativeInitialized = true;
        OmniboxActionFactoryImpl.get().initNativeFactory();
        mDropdownViewInfoListManager.onNativeInitialized();
        mDropdownViewInfoListBuilder.onNativeInitialized();
        runPendingAutocompleteRequests();
    }

    /**
     * Take necessary action to update the autocomplete system state and record metrics when the
     * omnibox session state changes.
     *
     * @param activated Whether the autocomplete session should be activated when the omnibox
     *     session state changes, {@code true} if this will be activated, {@code false} otherwise.
     */
    void onOmniboxSessionStateChange(boolean activated) {
        if (mIsActive == activated) return;
        mIsActive = activated;

        // Propagate the information about omnibox session state change to all the processors first.
        // Processors need this for accounting purposes.
        // The change information should be passed before Processors receive first
        // batch of suggestions, that is:
        // - before any call to startZeroSuggest() (when first suggestions are populated), and
        // - before stopAutocomplete() (when current suggestions are erased).
        mDropdownViewInfoListBuilder.onOmniboxSessionStateChange(activated);

        if (mAnimationDriver != null && mAnimationDriver.isImeAnimationEnabled()) {
            mAnimationDriver.onOmniboxSessionStateChange(activated);
            if (activated) {
                mDelegate.setKeyboardVisibility(true, false);
            }
        }

        if (activated) {
            mDeferredIMEWindowInsetApplicationCallback.attach(mWindowAndroid);
            dismissDeleteDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
            mRefineActionUsage = RefineActionUsage.NOT_USED;
            mOmniboxFocusResultedInNavigation = false;
            mSuggestionsListScrolled = false;
            mPageClassification = OptionalInt.of(mDataProvider.getPageClassification(false));
            mUrlFocusTime = System.currentTimeMillis();

            // Ask directly for zero-suggestions related to current input, unless the user is
            // currently visiting SearchActivity and the input is populated from the launch intent.
            // In all contexts, the input will most likely be empty, triggering the same response
            // (starting zero suggestions), but if the SearchActivity was launched with a QUERY,
            // then the query might point to a different URL than the reported Page, and the
            // suggestion would take the user to the DSE home page.
            // This is tracked by MobileStartup.LaunchCause / EXTERNAL_SEARCH_ACTION_INTENT
            // metric.
            String text = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
            onTextChanged(
                    text, /* isOnFocusContext= */ OmniboxFeatures.shouldRetainOmniboxOnFocus());
        } else {
            mDeferredIMEWindowInsetApplicationCallback.detach();
            stopMeasuringSuggestionRequestToUiModelTime();
            cancelAutocompleteRequests();
            OmniboxMetrics.recordOmniboxFocusResultedInNavigation(
                    mOmniboxFocusResultedInNavigation);
            OmniboxMetrics.recordRefineActionUsage(mRefineActionUsage);
            OmniboxMetrics.recordSuggestionsListScrolled(
                    mPageClassification.getAsInt(), mSuggestionsListScrolled);
            mPageClassification = OptionalInt.empty();

            // Reset the per omnibox session state of touch down prefetch.
            OmniboxMetrics.recordNumPrefetchesStartedInOmniboxSession(
                    mNumPrefetchesStartedInOmniboxSession);
            mNumTouchDownEventForwardedInOmniboxSession = 0;
            mNumPrefetchesStartedInOmniboxSession = 0;
            mLastPrefetchStartedSuggestion = Optional.empty();

            mEditSessionState = EditSessionState.INACTIVE;
            mNewOmniboxEditSessionTimestamp = -1;
            // Prevent any upcoming omnibox suggestions from showing once a URL is loaded (and as
            // a consequence the omnibox is unfocused).
            clearSuggestions();
        }
    }

    /**
     * @see
     *     org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlAnimationFinished(boolean)
     */
    void onUrlAnimationFinished(boolean hasFocus) {
        // mAnimationDriver has the responsibility of calling propagateOmniboxSessionStateChange if
        // it's present and currently active.
        if (hasFocus && mAnimationDriver != null && mAnimationDriver.isImeAnimationEnabled()) {
            return;
        }
        propagateOmniboxSessionStateChange(hasFocus);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     *
     * @param profile The profile to be used.
     */
    void setAutocompleteProfile(Profile profile) {
        stopAutocomplete(true);
        mAutocomplete.ifPresent(a -> a.removeOnSuggestionsReceivedListener(this));
        mAutocomplete = AutocompleteController.getForProfile(profile);
        mAutocomplete.ifPresent(a -> a.addOnSuggestionsReceivedListener(this));
        mDropdownViewInfoListBuilder.setProfile(profile);
        runPendingAutocompleteRequests();
    }

    /** Whether omnibox autocomplete should currently be prevented from generating suggestions. */
    void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mShouldPreventOmniboxAutocomplete = prevent;
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results) {
        mAutocomplete.ifPresent(a -> a.onVoiceResults(results));
    }

    /**
     * TODO(crbug.com/40725530): Figure out how to remove this.
     *
     * @return The current native pointer to the autocomplete results.
     */
    long getCurrentNativeAutocompleteResult() {
        return mAutocompleteResult.map(r -> r.getNativeObjectRef()).orElse(0L);
    }

    /**
     * Triggered when the user selects one of the omnibox suggestions to navigate to.
     *
     * @param suggestion The AutocompleteMatch which was selected.
     * @param matchIndex Position of the suggestion in the drop down view.
     * @param url The URL associated with the suggestion.
     */
    @Override
    public void onSuggestionClicked(
            @NonNull AutocompleteMatch suggestion, int matchIndex, @NonNull GURL url) {
        mDeferredLoadAction =
                Optional.of(
                        () ->
                                loadUrlForOmniboxMatch(
                                        matchIndex,
                                        suggestion,
                                        url,
                                        mLastActionUpTimestamp,
                                        /* openInNewTab= */ false,
                                        true));

        // Note: Action will be reset when load is initiated.
        mAutocomplete.ifPresent(a -> mDeferredLoadAction.get().run());
    }

    /**
     * Triggered when the user touches down on a search suggestion.
     *
     * @param suggestion The AutocompleteMatch which was selected.
     * @param matchIndex Position of the suggestion in the drop down view.
     */
    @Override
    public void onSuggestionTouchDown(@NonNull AutocompleteMatch suggestion, int matchIndex) {
        if (mAutocomplete.isEmpty()
                || mNumTouchDownEventForwardedInOmniboxSession
                        >= OmniboxFeatures.getMaxPrefetchesPerOmniboxSession()) {
            return;
        }
        mNumTouchDownEventForwardedInOmniboxSession++;

        var tab = mDataProvider.getTab();
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        boolean wasPrefetchStarted =
                mAutocomplete
                        .map(a -> a.onSuggestionTouchDown(suggestion, matchIndex, webContents))
                        .orElse(false);
        if (wasPrefetchStarted) {
            mNumPrefetchesStartedInOmniboxSession++;
            mLastPrefetchStartedSuggestion = Optional.of(suggestion);
        }
    }

    @Override
    public void onOmniboxActionClicked(@NonNull OmniboxAction action, int position) {
        if (action instanceof OmniboxAnswerAction omniboxAnswerAction) {
            Optional<AutocompleteMatch> associatedSuggestion =
                    mAutocompleteResult
                            .map(AutocompleteResult::getSuggestionsList)
                            .map((list) -> list.get(position));
            if (!associatedSuggestion.isPresent()) {
                return;
            }

            // Allow the action to record execution-related metrics before we navigate away.
            action.execute(mOmniboxActionDelegate);
            // onSuggestionClicked will post a call to finishInteraction, so we don't need to call
            // it immediately.
            loadUrlForOmniboxMatch(
                    0,
                    associatedSuggestion.get(),
                    mAutocomplete
                            .map(
                                    a ->
                                            a.getAnswerActionDestinationURL(
                                                    associatedSuggestion.get(),
                                                    mLastActionUpTimestamp,
                                                    omniboxAnswerAction))
                            .orElse(associatedSuggestion.get().getUrl()),
                    getElapsedTimeSinceInputChange(),
                    false,
                    false);
        } else {
            action.execute(mOmniboxActionDelegate);
            finishInteraction();
        }
    }

    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     *
     * @param suggestion The suggestion selected.
     */
    @Override
    public void onRefineSuggestion(@NonNull AutocompleteMatch suggestion) {
        stopAutocomplete(false);
        boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        boolean isZeroPrefix =
                TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithoutAutocomplete());
        String refineText = suggestion.getFillIntoEdit();
        if (isSearchSuggestion) refineText = TextUtils.concat(refineText, " ").toString();

        mDelegate.setOmniboxEditingText(refineText);
        onTextChanged(
                mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                /* isOnFocusContext= */ false);

        if (isSearchSuggestion) {
            // Note: the logic below toggles assumes individual values to be represented by
            // individual bits. This allows proper reporting of different refine button uses
            // during single interaction with the Omnibox.
            mRefineActionUsage |=
                    isZeroPrefix
                            ? RefineActionUsage.SEARCH_WITH_ZERO_PREFIX
                            : RefineActionUsage.SEARCH_WITH_PREFIX;
        }
    }

    @Override
    public void onSwitchToTab(@NonNull AutocompleteMatch match, int matchIndex) {
        if (maybeSwitchToTab(match)) {
            recordMetrics(match, matchIndex, WindowOpenDisposition.SWITCH_TO_TAB);
        } else {
            onSuggestionClicked(match, matchIndex, match.getUrl());
        }
    }

    @VisibleForTesting
    public boolean maybeSwitchToTab(AutocompleteMatch match) {
        Tab tab = mAutocomplete.map(a -> a.getMatchingTabForSuggestion(match)).orElse(null);
        if (tab == null || !mTabWindowManagerSupplier.hasValue()) return false;

        // When invoked directly from a browser, we want to trigger switch to tab animation.
        // If invoked from other activities, ex. searchActivity, we do not need to trigger the
        // animation since Android will show the animation for switching apps.
        if (tab.getWindowAndroid().getActivityState() == ActivityState.STOPPED
                || tab.getWindowAndroid().getActivityState() == ActivityState.DESTROYED) {
            mBringTabToFrontCallback.onResult(tab);
            return true;
        }

        TabModel tabModel = mTabWindowManagerSupplier.get().getTabModelForTab(tab);
        if (tabModel == null) return false;

        int tabIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
        // In the event the user deleted the tab as part during the interaction with the
        // Omnibox, reject the switch to tab action.
        if (tabIndex == TabModel.INVALID_TAB_INDEX) return false;
        tabModel.setIndex(tabIndex, TabSelectionType.FROM_OMNIBOX);
        return true;
    }

    @Override
    public void onGesture(boolean isGestureUp, long timestamp) {
        stopAutocomplete(false);
        if (isGestureUp) {
            mLastActionUpTimestamp = timestamp;
        }
    }

    /**
     * Triggered when the user long presses the omnibox suggestion.
     *
     * @param suggestion The suggestion selected.
     * @param titleText The title to display in the delete dialog.
     */
    @Override
    public void onDeleteMatch(@NonNull AutocompleteMatch suggestion, @NonNull String titleText) {
        showDeleteDialog(
                suggestion,
                titleText,
                () -> mAutocomplete.ifPresent(a -> a.deleteMatch(suggestion)));
    }

    /**
     * Triggered when the user long presses the omnibox suggestion element (eg. a tile).
     *
     * @param suggestion The suggestion selected.
     * @param titleText The title to display in the delete dialog.
     * @param elementIndex The element of the suggestion to be deleted.
     */
    @Override
    public void onDeleteMatchElement(
            @NonNull AutocompleteMatch suggestion, @NonNull String titleText, int elementIndex) {
        showDeleteDialog(
                suggestion,
                titleText,
                () -> mAutocomplete.ifPresent(a -> a.deleteMatchElement(suggestion, elementIndex)));
    }

    /** Terminate the interaction with the Omnibox. */
    @Override
    public void finishInteraction() {
        mDelegate.clearOmniboxFocus();
    }

    public void showDeleteDialog(
            @NonNull AutocompleteMatch suggestion,
            @NonNull String titleText,
            Runnable deleteAction) {
        RecordUserAction.record("MobileOmniboxDeleteGesture");

        // Prevent updates to the shown omnibox suggestions list while the dialog is open.
        // Each update invalidates previous result set, making it impossible to perform the delete
        // action (there is no native match to delete). Calling `stopAutocomplete()` here will
        // ensure that suggestions don't change the moment the User is presented with the dialog,
        // allowing us to complete the deletion.
        stopAutocomplete(/* clear= */ false);
        if (!suggestion.isDeletable()) return;
        // Do not attempt to delete matches that have been detached from their native counterpart.
        // These matches likely come from cache, or the delete request came for a previous set of
        // matches.
        if (suggestion.getNativeObjectRef() == 0) return;

        ModalDialogManager manager = mModalDialogManagerSupplier.get();
        if (manager == null) {
            assert false : "No modal dialog manager registered for this activity.";
            return;
        }

        ModalDialogProperties.Controller dialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            RecordUserAction.record("MobileOmniboxDeleteRequested");
                            deleteAction.run();
                            manager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                            manager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        mDeleteDialogModel = Optional.empty();
                    }
                };

        Resources resources = mContext.getResources();
        @StringRes int dialogMessageId = R.string.omnibox_confirm_delete;
        if (isSuggestionFromClipboard(suggestion)) {
            dialogMessageId = R.string.omnibox_confirm_delete_from_clipboard;
        }

        mDeleteDialogModel =
                Optional.of(
                        new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                .with(ModalDialogProperties.CONTROLLER, dialogController)
                                .with(ModalDialogProperties.TITLE, titleText)
                                .with(ModalDialogProperties.TITLE_MAX_LINES, 1)
                                .with(
                                        ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                        resources.getString(dialogMessageId))
                                .with(
                                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                        resources,
                                        R.string.ok)
                                .with(
                                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                        resources,
                                        R.string.cancel)
                                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                                .build());

        manager.showDialog(mDeleteDialogModel.get(), ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Dismiss the delete suggestion dialog if it is showing.
     *
     * @param cause The cause of dismiss.
     */
    private void dismissDeleteDialog(@DialogDismissalCause int cause) {
        var manager = mModalDialogManagerSupplier.get();
        mDeleteDialogModel.ifPresent(model -> manager.dismissDialog(model, cause));
    }

    /**
     * Triggered when the user navigates to one of the suggestions without clicking on it.
     *
     * @param text The text to be displayed in the Omnibox.
     */
    @Override
    public void setOmniboxEditingText(@NonNull String text) {
        if (mIgnoreOmniboxItemSelection) return;
        mIgnoreOmniboxItemSelection = true;
        mDelegate.setOmniboxEditingText(text);
    }

    /**
     * Updates the URL we will navigate to from suggestion, if needed. This will update the search
     * URL to be of the corpus type if query in the omnibox is displayed and update gs_lcrp=
     * parameter on regular web search URLs.
     *
     * @param suggestion The chosen omnibox suggestion.
     * @param matchIndex The index of the chosen omnibox suggestion.
     * @param url The URL associated with the suggestion to navigate to.
     * @return The url to navigate to.
     */
    private @NonNull GURL updateSuggestionUrlIfNeeded(
            @NonNull AutocompleteMatch suggestion, @NonNull GURL url) {
        if (mAutocomplete.isEmpty()) return url;
        // TODO(crbug.com/40279214): this should exclude TILE variants when horizontal render group
        // is
        // ready.
        if (suggestion.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST) {
            return url;
        }

        return mAutocomplete
                .map(
                        a ->
                                a.updateMatchDestinationUrlWithQueryFormulationTime(
                                        suggestion, getElapsedTimeSinceInputChange()))
                .orElse(url);
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     *
     * @param textWithoutAutocomplete The text that does not include autocomplete information.
     * @param isOnFocusContext Whether the request is made on focus (as opposed to on text change).
     */
    public void onTextChanged(@NonNull String textWithoutAutocomplete, boolean isOnFocusContext) {
        if (mShouldPreventOmniboxAutocomplete) return;

        mIgnoreOmniboxItemSelection = true;
        cancelAutocompleteRequests();

        if (mEditSessionState == EditSessionState.INACTIVE) {
            mAutocomplete.ifPresent(a -> a.resetSession());
            mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
            mEditSessionState = EditSessionState.ACTIVATED_BY_USER_INPUT;
        }

        stopAutocomplete(false);

        boolean isTextWithoutAutocompleteEmpty = TextUtils.isEmpty(textWithoutAutocomplete);
        mIsInZeroPrefixContext = isOnFocusContext || isTextWithoutAutocompleteEmpty;
        isOnFocusContext = isOnFocusContext && !isTextWithoutAutocompleteEmpty;

        if (mIsInZeroPrefixContext) {
            clearSuggestions();
            startCachedZeroSuggest(isOnFocusContext);
        } else if (mDataProvider.hasTab()) {
            boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
            int cursorPosition =
                    mUrlBarEditingTextProvider.getSelectionStart()
                                    == mUrlBarEditingTextProvider.getSelectionEnd()
                            ? mUrlBarEditingTextProvider.getSelectionStart()
                            : -1;
            GURL currentUrl = mDataProvider.getCurrentGurl();

            postAutocompleteRequest(
                    () -> {
                        if (!mPageClassification.isPresent()) return;
                        startMeasuringSuggestionRequestToUiModelTime();
                        mAutocomplete.ifPresent(
                                a ->
                                        a.start(
                                                currentUrl,
                                                mPageClassification.getAsInt(),
                                                textWithoutAutocomplete,
                                                cursorPosition,
                                                preventAutocomplete));
                    },
                    OMNIBOX_SUGGESTION_START_DELAY_MS);
        }

        mDelegate.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsReceived(
            @NonNull AutocompleteResult autocompleteResult, boolean isFinal) {
        // Reject results if the current session is inactive.
        if (!mIsActive) return;

        maybeCacheResult(autocompleteResult);

        @Nullable AutocompleteMatch defaultMatch = autocompleteResult.getDefaultMatch();
        String inlineAutocompleteText =
                defaultMatch != null ? defaultMatch.getInlineAutocompletion() : "";

        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        if (!mAutocompleteResult.map(r -> r.equals(autocompleteResult)).orElse(false)) {
            mAutocompleteResult = Optional.of(autocompleteResult);
            var viewInfoList =
                    mDropdownViewInfoListBuilder.buildDropdownViewInfoList(autocompleteResult);
            mDropdownViewInfoListManager.setSourceViewInfoList(viewInfoList);
            if (mIsActive) {
                mDelegate.onSuggestionsChanged(defaultMatch);
            }
        }

        mListPropertyModel.set(SuggestionListProperties.LIST_IS_FINAL, isFinal);
        measureSuggestionRequestToUiModelTime(isFinal);
    }

    /**
     * Load the url corresponding to the typed omnibox text.
     *
     * @param eventTime The timestamp the load was triggered by the user.
     * @param openInNewTab Whether the URL will be loaded in a new tab. If {@code true}, the URL
     *     will be loaded in a new tab. If {@code false}, The URL will be loaded in the current tab.
     */
    void loadTypedOmniboxText(long eventTime, boolean openInNewTab) {
        final String urlText = mUrlBarEditingTextProvider.getTextWithAutocomplete();
        cancelAutocompleteRequests();
        if (mAutocomplete.isPresent()) {
            findMatchAndLoadUrl(urlText, eventTime, openInNewTab);
        } else {
            mDeferredLoadAction =
                    Optional.of(() -> findMatchAndLoadUrl(urlText, eventTime, openInNewTab));
        }
    }

    /**
     * Search for a suggestion with the same associated URL as the supplied one.
     *
     * @param urlText The URL text to search for.
     * @param inputStart The timestamp the load was triggered by the user.
     * @param openInNewTab Whether the URL will be loaded in a new tab. If {@code true}, the URL
     *     will be loaded in a new tab. If {@code false}, The URL will be loaded in the current tab.
     */
    private void findMatchAndLoadUrl(
            @NonNull String urlText, long inputStart, boolean openInNewTab) {
        AutocompleteMatch suggestionMatch;

        if (getSuggestionCount() > 0
                && urlText.trim().equals(mUrlTextAfterSuggestionsReceived.trim())) {
            // Common case: the user typed something, received suggestions, then pressed enter.
            // This triggers the Default Match.
            suggestionMatch = getSuggestionAt(0);
        } else {
            // Less common case: there are no valid omnibox suggestions. This can happen if the
            // user tapped the URL bar to dismiss the suggestions, then pressed enter. This can
            // also happen if the user presses enter before any suggestions have been received
            // from the autocomplete controller.
            suggestionMatch = mAutocomplete.map(a -> a.classify(urlText)).orElse(null);
            // If urlText couldn't be classified, bail.
            if (suggestionMatch == null) return;
        }

        loadUrlForOmniboxMatch(
                0, suggestionMatch, suggestionMatch.getUrl(), inputStart, openInNewTab, true);
    }

    /**
     * Loads the specified omnibox suggestion.
     *
     * @param matchIndex The position of the selected omnibox suggestion.
     * @param suggestion The suggestion selected.
     * @param url The URL to load.
     * @param inputStart The timestamp the input was started.
     * @param openInNewTab Whether the suggestion will be loaded in a new tab. If {@code true}, the
     *     suggestion will be loaded in a new tab. If {@code false}, the suggestion will be loaded
     *     in the current tab.
     */
    private void loadUrlForOmniboxMatch(
            int matchIndex,
            @NonNull AutocompleteMatch suggestion,
            @NonNull GURL url,
            long inputStart,
            boolean openInNewTab,
            boolean shouldUpdateSuggestionUrl) {
        try (TraceEvent e = TraceEvent.scoped("AutocompleteMediator.loadUrlFromOmniboxMatch")) {
            OmniboxMetrics.recordFocusToOpenTime(System.currentTimeMillis() - mUrlFocusTime);

            // Clear the deferred site load action in case it executes. Reclaims a bit of memory.
            mDeferredLoadAction = Optional.empty();

            mOmniboxFocusResultedInNavigation = true;
            if (shouldUpdateSuggestionUrl) {
                url = updateSuggestionUrlIfNeeded(suggestion, url);
            }

            // loadUrl modifies AutocompleteController's state clearing the native
            // AutocompleteResults needed by onSuggestionsSelected. Therefore,
            // loadUrl should should be invoked last.
            int transition = suggestion.getTransition();
            int type = suggestion.getType();

            recordMetrics(suggestion, matchIndex, WindowOpenDisposition.CURRENT_TAB);
            if (((transition & PageTransition.CORE_MASK) == PageTransition.TYPED)
                    && url.equals(mDataProvider.getCurrentGurl())) {
                // When the user hit enter on the existing permanent URL, treat it like a
                // reload for scoring purposes.  We could detect this by just checking
                // user_input_in_progress_, but it seems better to treat "edits" that end
                // up leaving the URL unchanged (e.g. deleting the last character and then
                // retyping it) as reloads too.  We exclude non-TYPED transitions because if
                // the transition is GENERATED, the user input something that looked
                // different from the current URL, even if it wound up at the same place
                // (e.g. manually retyping the same search query), and it seems wrong to
                // treat this as a reload.
                transition = PageTransition.RELOAD;
            } else if (type == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                    && mUrlBarEditingTextProvider.wasLastEditPaste()) {
                // It's important to use the page transition from the suggestion or we might end
                // up saving generated URLs as typed URLs, which would then pollute the subsequent
                // omnibox results. There is one special case where the suggestion text was pasted,
                // where we want the transition type to be LINK.

                transition = PageTransition.LINK;
            }

            if (isSuggestionLinkFromClipboard(suggestion)) {
                mDelegate.maybeShowDefaultBrowserPromo();
            }

            // Kick off an action to clear focus and dismiss the suggestions list.
            // This normally happens when the target site loads and focus is moved to the
            // webcontents. On Android T we occasionally observe focus events to be lost, resulting
            // with Suggestions list obscuring the view.
            var autocompleteLoadCallback =
                    new AutocompleteLoadCallback() {
                        @Override
                        public void onLoadUrl(LoadUrlParams params, LoadUrlResult loadUrlResult) {
                            if (loadUrlResult.navigationHandle != null) {
                                mAutocomplete.ifPresent(
                                        a ->
                                                a.createNavigationObserver(
                                                        loadUrlResult.navigationHandle,
                                                        suggestion));
                            }
                        }
                    };

            if (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                mDelegate.loadUrl(
                        new OmniboxLoadUrlParams.Builder(url.getSpec(), transition)
                                .setInputStartTimestamp(inputStart)
                                .setpostDataAndType(
                                        suggestion.getPostData(), suggestion.getPostContentType())
                                .setAutocompleteLoadCallback(autocompleteLoadCallback)
                                .build());
            } else {
                mDelegate.loadUrl(
                        new OmniboxLoadUrlParams.Builder(url.getSpec(), transition)
                                .setInputStartTimestamp(inputStart)
                                .setOpenInNewTab(openInNewTab)
                                .setAutocompleteLoadCallback(autocompleteLoadCallback)
                                .build());
            }

            mHandler.post(this::finishInteraction);
        }
    }

    /** Sends a zero suggest request to the server in order to pre-populate the result cache. */
    /* package */ void startPrefetch() {
        int pageClassification = mDataProvider.getPageClassification(true);
        postAutocompleteRequest(
                () ->
                        mAutocomplete.ifPresent(
                                a ->
                                        a.startPrefetch(
                                                mDataProvider.getCurrentGurl(),
                                                pageClassification)),
                SCHEDULE_FOR_IMMEDIATE_EXECUTION);
    }

    /**
     * Make a zero suggest request if: - The URL bar has focus. - The the tab/overview is not
     * incognito. This method should not be called directly. Schedule execution using
     * postAutocompleteRequest.
     *
     * @param isOnFocusContext Whether the request is made on focus (as opposed to on text change).
     */
    private void startZeroSuggest(boolean isOnFocusContext) {
        // Reset "edited" state in the omnibox if zero suggest is triggered -- new edits
        // now count as a new session.
        mEditSessionState = EditSessionState.INACTIVE;
        mNewOmniboxEditSessionTimestamp = -1;
        startMeasuringSuggestionRequestToUiModelTime();

        if (mDelegate.isUrlBarFocused() && mDataProvider.hasTab()) {
            mAutocomplete.ifPresent(
                    a -> {
                        if (!mPageClassification.isPresent()) return;
                        a.startZeroSuggest(
                                mUrlBarEditingTextProvider.getTextWithAutocomplete(),
                                mDataProvider.getCurrentGurl(),
                                mPageClassification.getAsInt(),
                                mDataProvider.getTitle(),
                                isOnFocusContext);
                    });
        }
    }

    /**
     * Update whether the Omnibox session is active.
     *
     * @param isActive whether session is currently active
     */
    @VisibleForTesting
    void propagateOmniboxSessionStateChange(boolean isActive) {
        boolean wasActive = mListPropertyModel.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE);
        mListPropertyModel.set(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, isActive);

        if (isActive != wasActive) {
            mIgnoreOmniboxItemSelection |= isActive; // Reset to default value.
            mOmniboxSuggestionsVisualStateObserver.ifPresent(
                    (observer) -> observer.onOmniboxSessionStateChange(isActive));
        }
    }

    /**
     * Clear the list of suggestions.
     *
     * <p>This call is used to terminate the Autocomplete session and hide the suggestions list
     * while the Omnibox session is active.
     *
     * <p>This call *does not* terminate the Omnibox session.
     *
     * @see the {@link AutocompleteController#stop(boolean)}
     */
    @VisibleForTesting
    void clearSuggestions() {
        stopAutocomplete(true);
        dismissDeleteDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        mDropdownViewInfoListManager.clear();
        mAutocompleteResult = Optional.empty();
    }

    /**
     * Signals the autocomplete controller to stop generating omnibox suggestions and cancels the
     * queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void stopAutocomplete(boolean clear) {
        mAutocomplete.ifPresent(a -> a.stop(clear));
        cancelAutocompleteRequests();
    }

    /** Trigger autocomplete for the given query. */
    void startAutocompleteForQuery(@NonNull String query) {
        stopAutocomplete(false);
        if (mDataProvider.hasTab()) {
            mAutocomplete.ifPresent(
                    a ->
                            a.start(
                                    mDataProvider.getCurrentGurl(),
                                    mDataProvider.getPageClassification(false),
                                    query,
                                    -1,
                                    false));
        }
    }

    /**
     * Respond to Suggestion list height change and update list of presented suggestions.
     *
     * <p>This typically happens as a result of soft keyboard being shown or hidden.
     *
     * @param newHeight New height of the suggestion list in pixels.
     */
    public void onSuggestionDropdownHeightChanged(@Px int newHeight) {
        // Report the dropdown height whenever we intend to - or do show soft keyboard. This
        // addresses cases where hardware keyboard is attached to a device, or where user explicitly
        // called the keyboard back after we hid it.
        if (mDelegate.isKeyboardActive()) {
            int suggestionHeight =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.omnibox_suggestion_content_height);
            mAutocomplete.ifPresent(
                    a -> a.onSuggestionDropdownHeightChanged(newHeight, suggestionHeight));
        }
    }

    @Override
    public void onSuggestionDropdownScroll() {
        mSuggestionsListScrolled = true;
        mDelegate.setKeyboardVisibility(false, false);
    }

    /**
     * Called whenever a navigation happens from the omnibox to record metrics about the user's
     * interaction with the omnibox.
     *
     * @param match the selected AutocompleteMatch
     * @param suggestionLine the index of the suggestion line that holds selected match
     * @param disposition the window open disposition
     */
    private void recordMetrics(
            @NonNull AutocompleteMatch match, int suggestionLine, int disposition) {
        if (mAutocompleteResult.isEmpty()) return;

        boolean autocompleteResultIsFromCache =
                mAutocompleteResult.map(r -> r.isFromCachedResult()).orElse(true);

        OmniboxMetrics.recordUsedSuggestionFromCache(autocompleteResultIsFromCache);
        OmniboxMetrics.recordTouchDownPrefetchResult(match, mLastPrefetchStartedSuggestion);

        // Do not attempt to record other metrics for cached suggestions if the source of the list
        // is local cache. These suggestions do not have corresponding native objects and will fail
        // validation.
        if (autocompleteResultIsFromCache) return;

        GURL currentPageUrl = mDataProvider.getCurrentGurl();
        long elapsedTimeSinceModified = getElapsedTimeSinceInputChange();
        int autocompleteLength =
                mUrlBarEditingTextProvider.getTextWithAutocomplete().length()
                        - mUrlBarEditingTextProvider.getTextWithoutAutocomplete().length();
        var tab = mDataProvider.getTab();
        WebContents webContents = tab != null ? tab.getWebContents() : null;

        mAutocomplete.ifPresent(
                a ->
                        a.onSuggestionSelected(
                                match,
                                suggestionLine,
                                disposition,
                                currentPageUrl,
                                mPageClassification.getAsInt(),
                                elapsedTimeSinceModified,
                                autocompleteLength,
                                webContents));
    }

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {
        mDelegate.setKeyboardVisibility(true, false);
    }

    /**
     * @return elapsed time (in milliseconds) since last input or -1 if user has chosen a
     *     zero-prefix suggestion.
     */
    private long getElapsedTimeSinceInputChange() {
        return mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @EditSessionState
    int getEditSessionStateForTest() {
        return mEditSessionState;
    }

    /**
     * Schedule Autocomplete action for execution. Each Autocomplete action posted here will cancel
     * any previously posted Autocomplete action, ensuring that the actions don't compete against
     * each other. Any action scheduled for execution before Native libraries are ready will be
     * deferred.
     *
     * <p>This call should only be used for regular suggest flows. Do not post arbitrary tasks here.
     *
     * @param action Autocomplete action to execute.
     * @param delayMillis The number of milliseconds by which the action should be delayed. Use
     *     SCHEDULE_FOR_IMMEDIATE_EXECUTION to post action at front of the message queue.
     */
    private void postAutocompleteRequest(@NonNull Runnable action, long delayMillis) {
        assert !mIsExecutingAutocompleteAction : "Can't schedule conflicting autocomplete action";
        assert ThreadUtils.runningOnUiThread() : "Detected input from a non-UI thread. Test error?";

        cancelAutocompleteRequests();
        mCurrentAutocompleteRequest =
                Optional.of(
                        new Runnable() {
                            @Override
                            public void run() {
                                mIsExecutingAutocompleteAction = true;
                                action.run();
                                mIsExecutingAutocompleteAction = false;
                                // Release completed Runnable.
                                mCurrentAutocompleteRequest = Optional.empty();
                            }
                        });

        // In the event we got Native Ready signal but no Profile yet (or the other way around),
        // delay execution of the Autocomplete request.
        if (!mNativeInitialized || mAutocomplete.isEmpty()) return;
        mCurrentAutocompleteRequest.ifPresent(
                request -> {
                    if (delayMillis == SCHEDULE_FOR_IMMEDIATE_EXECUTION) {
                        // TODO(crbug.com/40167699): Replace the following with postAtFrontOfQueue()
                        // and correct any tests that expect data instantly.
                        request.run();
                    } else {
                        mHandler.postDelayed(request, delayMillis);
                    }
                });
    }

    /** Cancel any pending autocomplete actions. */
    private void cancelAutocompleteRequests() {
        stopMeasuringSuggestionRequestToUiModelTime();
        mCurrentAutocompleteRequest.ifPresent(r -> mHandler.removeCallbacks(r));
        mCurrentAutocompleteRequest = Optional.empty();
    }

    /** Execute any pending Autocomplete requests, if the Autocomplete subsystem is ready. */
    private void runPendingAutocompleteRequests() {
        if (!mNativeInitialized || mAutocomplete.isEmpty()) return;

        mDeferredLoadAction
                // If deferred load action is present, cancel all autocomplete and load the URL.
                .map(
                        action -> {
                            cancelAutocompleteRequests();
                            return action;
                        })
                // Otherwise, run pending autocomplete action (if any).
                .or(() -> mCurrentAutocompleteRequest)
                .ifPresent(runnable -> mHandler.postAtFrontOfQueue(runnable));
    }

    /**
     * Start measuring time between - the request for suggestions and - the suggestions UI model
     * being built. This should be invoked right before we issue a request for suggestions.
     */
    private void startMeasuringSuggestionRequestToUiModelTime() {
        mLastSuggestionRequestTime = SystemClock.uptimeMillis();
        mFirstSuggestionListModelCreatedTime = null;
    }

    /**
     * Measure the time it took to build Suggestions UI model. The time is measured since the moment
     * suggestions were requested. Two histograms are recorded by this method:
     *
     * <ul>
     *   <li>Omnibox.SuggestionList.RequestToUiModel.First for the first reply associated with the
     *       request and
     *   <li>Omnibox.SuggestionList.RequestToUiModel.Last for the final reply associated with the
     *       request.
     * </ul>
     *
     * Any other replies that happen meantime are ignored and are accounted for by the last/final
     * measurement.
     *
     * @param isFinal whether the measurement is for the final suggestions repsponse
     */
    private void measureSuggestionRequestToUiModelTime(boolean isFinal) {
        if (mLastSuggestionRequestTime == null) return;

        if (mFirstSuggestionListModelCreatedTime == null) {
            mFirstSuggestionListModelCreatedTime = SystemClock.uptimeMillis();
            OmniboxMetrics.recordSuggestionRequestToModelTime(
                    /* isFirst= */ true,
                    mFirstSuggestionListModelCreatedTime - mLastSuggestionRequestTime);
        }

        if (isFinal) {
            OmniboxMetrics.recordSuggestionRequestToModelTime(
                    /* isFirst= */ false, SystemClock.uptimeMillis() - mLastSuggestionRequestTime);
            stopMeasuringSuggestionRequestToUiModelTime();
        }
    }

    /** Cancel any measurements related to the time it takes to build Suggestions UI model. */
    private void stopMeasuringSuggestionRequestToUiModelTime() {
        mLastSuggestionRequestTime = null;
        mFirstSuggestionListModelCreatedTime = null;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    SuggestionsListAnimationDriver initializeAnimationDriver(Window window) {
        int addedVerticalOffset =
                mContext.getResources()
                        .getDimensionPixelOffset(
                                R.dimen.omnibox_suggestion_list_animation_added_vertical_offset);
        mAnimationDriver =
                new SuggestionsListAnimationDriver(
                        mWindowAndroid.getInsetObserver(),
                        mListPropertyModel,
                        mEmbedder::getVerticalTranslationForAnimation,
                        () -> propagateOmniboxSessionStateChange(true),
                        addedVerticalOffset,
                        new Handler(),
                        window);
        return mAnimationDriver;
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        // TODO(crbug.com/329702834): Ensuring showing Suggestions when activity resumes.
        if (!isTopResumedActivity) {
            clearSuggestions();
            mDelegate.clearOmniboxFocus();
        }
    }
}
