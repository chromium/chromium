// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics.RefineActionUsage;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionFactoryImpl;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.history_clusters.HistoryClustersProcessor.OpenHistoryClustersDelegate;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
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

/** Handles updating the model state for the currently visible omnibox suggestions. */
class AutocompleteMediator
        implements OnSuggestionsReceivedListener,
                OmniboxSuggestionsDropdown.GestureObserver,
                OmniboxSuggestionsDropdownScrollListener,
                SuggestionHost {
    private static final int SUGGESTION_NOT_FOUND = -1;
    private static final int SCHEDULE_FOR_IMMEDIATE_EXECUTION = -1;

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;

    private final @NonNull Context mContext;
    private final @NonNull AutocompleteControllerProvider mControllerProvider;
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
    private final @NonNull Runnable mClearFocusCallback;
    private final @NonNull OmniboxActionDelegate mOmniboxActionDelegate;

    private @NonNull AutocompleteResult mAutocompleteResult = AutocompleteResult.EMPTY_RESULT;
    private @Nullable Runnable mCurrentAutocompleteRequest;
    private @Nullable Runnable mDeferredLoadAction;
    private @Nullable PropertyModel mDeleteDialogModel;
    private @Nullable TemplateUrlService mTemplateUrlService;

    private boolean mNativeInitialized;
    private AutocompleteController mAutocomplete;
    private long mUrlFocusTime;
    private boolean mShouldCacheSuggestions;
    private boolean mClearFocusAfterNavigation;
    private boolean mClearFocusAfterNavigationAsynchronously;
    // When set, indicates an active omnibox session.
    private boolean mIsActive;
    // When set, specifies the system time of the most recent suggestion list request.
    private Long mLastSuggestionRequestTime;
    // When set, specifies the time when the suggestion list was shown the first time.
    // Suggestions are refreshed several times per keystroke.
    private Long mFirstSuggestionListModelCreatedTime;

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
    private @Nullable AutocompleteMatch mLastPrefetchStartedSuggestion;

    public AutocompleteMediator(
            @NonNull Context context,
            @NonNull AutocompleteControllerProvider controllerProvider,
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
            @NonNull OpenHistoryClustersDelegate openHistoryClustersDelegate) {
        mContext = context;
        mControllerProvider = controllerProvider;
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
        mDropdownViewInfoListBuilder =
                new DropdownItemViewInfoListBuilder(
                        activityTabSupplier, bookmarkState, openHistoryClustersDelegate);
        mDropdownViewInfoListBuilder.setShareDelegateSupplier(shareDelegateSupplier);
        mDropdownViewInfoListManager =
                new DropdownItemViewInfoListManager(mSuggestionModels, context);
        mClearFocusCallback = this::finishInteraction;
        OmniboxResourceProvider.invalidateDrawableCache();

        var pm = context.getPackageManager();
        var dialIntent = new Intent(Intent.ACTION_DIAL);
        OmniboxActionFactoryImpl.get()
                .setDialerAvailable(!pm.queryIntentActivities(dialIntent, 0).isEmpty());
        mListPropertyModel.set(
                SuggestionListProperties.DRAW_OVER_ANCHOR,
                OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext)
                        && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
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
        if (mAutocomplete != null) {
            stopAutocomplete(false);
            mAutocomplete.removeOnSuggestionsReceivedListener(this);
        }
        if (mNativeInitialized) {
            OmniboxActionFactoryImpl.get().destroyNativeFactory();
        }
        mHandler.removeCallbacks(mClearFocusCallback);
        mDropdownViewInfoListBuilder.destroy();
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
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mAutocompleteResult.getSuggestionsList().size();
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
        return mAutocompleteResult.getSuggestionsList().get(matchIndex);
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
    }

    /**
     * Show cached zero suggest results. Enables Autocomplete subsystem to offer most recently
     * presented suggestions in the event where Native counterpart is not yet initialized.
     *
     * <p>Note: the only supported page context right now is the ANDROID_SEARCH_WIDGET.
     */
    void startCachedZeroSuggest() {
        if (mNativeInitialized) return;
        onSuggestionsReceived(CachedZeroSuggestionsManager.readFromCache(), "", true);
    }

    /** Notify the mediator that a item selection is pending and should be accepted. */
    void allowPendingItemSelection() {
        mIgnoreOmniboxItemSelection = false;
    }

    /** Signals that native initialization has completed. */
    void onNativeInitialized() {
        mNativeInitialized = true;
        OmniboxActionFactoryImpl.get().initNativeFactory();
        // TODO(b/277805322): remove this Feature and parameter once we've run a holdback
        // experiment.
        mClearFocusAfterNavigation =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CLEAR_OMNIBOX_FOCUS_AFTER_NAVIGATION);
        mClearFocusAfterNavigationAsynchronously =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CLEAR_OMNIBOX_FOCUS_AFTER_NAVIGATION,
                        "clear_focus_asynchronously",
                        true);
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

        if (activated) {
            dismissDeleteDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
            mRefineActionUsage = RefineActionUsage.NOT_USED;
            mOmniboxFocusResultedInNavigation = false;
            mSuggestionsListScrolled = false;
            mUrlFocusTime = System.currentTimeMillis();

            // Ask directly for zero-suggestions related to current input, unless the user is
            // currently visiting SearchActivity and the input is populated from the launch intent.
            // For SearchActivity, in most cases the input will be empty, triggering the same
            // response (starting zero suggestions), but if the Activity was launched with a QUERY,
            // then the query might point to a different URL than the reported Page, and the
            // suggestion would take the user to the DSE home page.
            // This is tracked by MobileStartup.LaunchCause / EXTERNAL_SEARCH_ACTION_INTENT
            // metric.
            if (mDataProvider.getPageClassification(
                            /* isFocusedFromFakebox= */ false, /* isPrefetch= */ false)
                    != PageClassification.ANDROID_SEARCH_WIDGET_VALUE) {
                postAutocompleteRequest(this::startZeroSuggest, SCHEDULE_FOR_IMMEDIATE_EXECUTION);
            } else {
                String text = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
                onTextChanged(text);
            }
        } else {
            stopMeasuringSuggestionRequestToUiModelTime();
            cancelAutocompleteRequests();
            OmniboxMetrics.recordOmniboxFocusResultedInNavigation(
                    mOmniboxFocusResultedInNavigation);
            OmniboxMetrics.recordRefineActionUsage(mRefineActionUsage);
            OmniboxMetrics.recordSuggestionsListScrolled(
                    mDataProvider.getPageClassification(
                            mDelegate.didFocusUrlFromFakebox(), /* isPrefetch= */ false),
                    mSuggestionsListScrolled);

            // Reset the per omnibox session state of touch down prefetch.
            OmniboxMetrics.recordNumPrefetchesStartedInOmniboxSession(
                    mNumPrefetchesStartedInOmniboxSession);
            mNumTouchDownEventForwardedInOmniboxSession = 0;
            mNumPrefetchesStartedInOmniboxSession = 0;
            mLastPrefetchStartedSuggestion = null;

            mEditSessionState = EditSessionState.INACTIVE;
            mNewOmniboxEditSessionTimestamp = -1;
            // Prevent any upcoming omnibox suggestions from showing once a URL is loaded (and as
            // a consequence the omnibox is unfocused).
            hideSuggestions();
        }
    }

    /**
     * @see
     *     org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlAnimationFinished(boolean)
     */
    void onUrlAnimationFinished(boolean hasFocus) {
        updateOmniboxSuggestionsVisibility(hasFocus);
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     *
     * @param profile The profile to be used.
     */
    void setAutocompleteProfile(Profile profile) {
        if (mAutocomplete != null) {
            stopAutocomplete(true);
            mAutocomplete.removeOnSuggestionsReceivedListener(this);
        }
        mAutocomplete = mControllerProvider.get(profile);
        mAutocomplete.addOnSuggestionsReceivedListener(this);
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
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
        if (!mNativeInitialized || mAutocomplete == null) return;
        mAutocomplete.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results. TODO(crbug.com/1138587):
     *     Figure out how to remove this.
     */
    long getCurrentNativeAutocompleteResult() {
        return mAutocompleteResult.getNativeObjectRef();
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
        if (!mNativeInitialized || mAutocomplete == null) {
            mDeferredLoadAction =
                    () ->
                            loadUrlForOmniboxMatch(
                                    matchIndex,
                                    suggestion,
                                    url,
                                    mLastActionUpTimestamp,
                                    /* openInNewTab= */ false);
            return;
        }

        loadUrlForOmniboxMatch(
                matchIndex, suggestion, url, mLastActionUpTimestamp, /* openInNewTab= */ false);
    }

    /**
     * Triggered when the user touches down on a search suggestion.
     *
     * @param suggestion The AutocompleteMatch which was selected.
     * @param matchIndex Position of the suggestion in the drop down view.
     */
    @Override
    public void onSuggestionTouchDown(@NonNull AutocompleteMatch suggestion, int matchIndex) {
        if (!mNativeInitialized
                || mAutocomplete == null
                || mNumTouchDownEventForwardedInOmniboxSession
                        >= OmniboxFeatures.getMaxPrefetchesPerOmniboxSession()) {
            return;
        }
        mNumTouchDownEventForwardedInOmniboxSession++;
        WebContents webContents =
                mDataProvider.hasTab() ? mDataProvider.getTab().getWebContents() : null;
        boolean wasPrefetchStarted =
                mAutocomplete.onSuggestionTouchDown(suggestion, matchIndex, webContents);
        if (wasPrefetchStarted) {
            mNumPrefetchesStartedInOmniboxSession++;
            mLastPrefetchStartedSuggestion = suggestion;
        }
    }

    @Override
    public void onOmniboxActionClicked(@NonNull OmniboxAction action) {
        action.execute(mOmniboxActionDelegate);
        finishInteraction();
    }

    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     *
     * @param suggestion The suggestion selected.
     */
    @Override
    public void onRefineSuggestion(AutocompleteMatch suggestion) {
        stopAutocomplete(false);
        boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        boolean isZeroPrefix =
                TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithoutAutocomplete());
        String refineText = suggestion.getFillIntoEdit();
        if (isSearchSuggestion) refineText = TextUtils.concat(refineText, " ").toString();

        mDelegate.setOmniboxEditingText(refineText);
        onTextChanged(mUrlBarEditingTextProvider.getTextWithoutAutocomplete());

        if (isSearchSuggestion) {
            // Note: the logic below toggles assumes individual values to be represented by
            // individual bits. This allows proper reporting of different refine button uses
            // during single interaction with the Omnibox.
            mRefineActionUsage |=
                    isZeroPrefix
                            ? RefineActionUsage.SEARCH_WITH_ZERO_PREFIX
                            : RefineActionUsage.SEARCH_WITH_PREFIX;
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
        } else {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
        }
    }

    @Override
    public void onSwitchToTab(AutocompleteMatch match, int matchIndex) {
        if (maybeSwitchToTab(match)) {
            recordMetrics(match, matchIndex, WindowOpenDisposition.SWITCH_TO_TAB);
        } else {
            onSuggestionClicked(match, matchIndex, match.getUrl());
        }
    }

    @VisibleForTesting
    public boolean maybeSwitchToTab(AutocompleteMatch match) {
        Tab tab = mAutocomplete.getMatchingTabForSuggestion(match);
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
        tabModel.setIndex(tabIndex, TabSelectionType.FROM_OMNIBOX, false);
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
        showDeleteDialog(suggestion, titleText, () -> mAutocomplete.deleteMatch(suggestion));
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
                () -> mAutocomplete.deleteMatchElement(suggestion, elementIndex));
    }

    /** Terminate the interaction with the Omnibox. */
    @Override
    public void finishInteraction() {
        mDelegate.clearOmniboxFocus();
    }

    @Override
    public @Nullable String queryFromGurl(GURL url) {
        if (mTemplateUrlService == null) return null;
        return mTemplateUrlService.getSearchQueryForUrl(url);
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
                        mDeleteDialogModel = null;
                    }
                };

        Resources resources = mContext.getResources();
        @StringRes int dialogMessageId = R.string.omnibox_confirm_delete;
        if (isSuggestionFromClipboard(suggestion)) {
            dialogMessageId = R.string.omnibox_confirm_delete_from_clipboard;
        }

        mDeleteDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, titleText)
                        .with(ModalDialogProperties.TITLE_MAX_LINES, 1)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(dialogMessageId))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.ok)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        manager.showDialog(mDeleteDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Dismiss the delete suggestion dialog if it is showing.
     *
     * @param cause The cause of dismiss.
     */
    private void dismissDeleteDialog(@DialogDismissalCause int cause) {
        if (mDeleteDialogModel == null) return;

        assert mModalDialogManagerSupplier.hasValue() : "Dialog shown with no registered manager";
        mModalDialogManagerSupplier.get().dismissDialog(mDeleteDialogModel, cause);
    }

    /**
     * Triggered when the user navigates to one of the suggestions without clicking on it.
     *
     * @param text The text to be displayed in the Omnibox.
     */
    @Override
    public void setOmniboxEditingText(String text) {
        if (mIgnoreOmniboxItemSelection) return;
        mIgnoreOmniboxItemSelection = true;
        mDelegate.setOmniboxEditingText(text);
    }

    /**
     * Updates the URL we will navigate to from suggestion, if needed. This will update the search
     * URL to be of the corpus type if query in the omnibox is displayed and update aqs= parameter
     * on regular web search URLs.
     *
     * @param suggestion The chosen omnibox suggestion.
     * @param matchIndex The index of the chosen omnibox suggestion.
     * @param url The URL associated with the suggestion to navigate to.
     * @return The url to navigate to.
     */
    private GURL updateSuggestionUrlIfNeeded(
            @NonNull AutocompleteMatch suggestion, int matchIndex, @NonNull GURL url) {
        if (!mNativeInitialized || mAutocomplete == null) return url;
        // TODO(crbug/1474087): this should exclude TILE variants when horizontal render group is
        // ready.
        if (suggestion.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST) {
            return url;
        }

        GURL updatedUrl =
                mAutocomplete.updateMatchDestinationUrlWithQueryFormulationTime(
                        suggestion, getElapsedTimeSinceInputChange());

        return updatedUrl == null ? url : updatedUrl;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     */
    public void onTextChanged(String textWithoutAutocomplete) {
        if (mShouldPreventOmniboxAutocomplete) return;

        mIgnoreOmniboxItemSelection = true;
        cancelAutocompleteRequests();

        if (mEditSessionState == EditSessionState.INACTIVE
                && mNativeInitialized
                && mAutocomplete != null) {
            mAutocomplete.resetSession();
            mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
            mEditSessionState = EditSessionState.ACTIVATED_BY_USER_INPUT;
        }

        stopAutocomplete(false);
        if (TextUtils.isEmpty(textWithoutAutocomplete)) {
            hideSuggestions();
            postAutocompleteRequest(this::startZeroSuggest, SCHEDULE_FOR_IMMEDIATE_EXECUTION);
        } else {
            // There may be no tabs when searching form omnibox in overview mode. In that case,
            // LocationBarDataProvider.getCurrentUrl() returns NTP url.
            if (mDataProvider.hasTab() || mDataProvider.isInOverviewAndShowingOmnibox()) {
                boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
                int cursorPosition =
                        mUrlBarEditingTextProvider.getSelectionStart()
                                        == mUrlBarEditingTextProvider.getSelectionEnd()
                                ? mUrlBarEditingTextProvider.getSelectionStart()
                                : -1;
                int pageClassification =
                        mDataProvider.getPageClassification(
                                mDelegate.didFocusUrlFromFakebox(), /* isPrefetch= */ false);
                GURL currentUrl = mDataProvider.getCurrentGurl();

                postAutocompleteRequest(
                        () -> {
                            startMeasuringSuggestionRequestToUiModelTime();
                            mAutocomplete.start(
                                    currentUrl,
                                    pageClassification,
                                    textWithoutAutocomplete,
                                    cursorPosition,
                                    preventAutocomplete);
                        },
                        OMNIBOX_SUGGESTION_START_DELAY_MS);
            }
        }

        mDelegate.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsReceived(
            AutocompleteResult autocompleteResult, String inlineAutocompleteText, boolean isFinal) {
        if (mShouldCacheSuggestions) {
            CachedZeroSuggestionsManager.saveToCache(autocompleteResult);
        }

        final List<AutocompleteMatch> newSuggestions = autocompleteResult.getSuggestionsList();
        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        if (!mAutocompleteResult.equals(autocompleteResult)) {
            mAutocompleteResult = autocompleteResult;
            var viewInfoList =
                    mDropdownViewInfoListBuilder.buildDropdownViewInfoList(autocompleteResult);
            mDropdownViewInfoListManager.setSourceViewInfoList(viewInfoList);
            boolean defaultMatchIsSearch = true;
            if (!TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithoutAutocomplete())
                    && !newSuggestions.isEmpty()) {
                defaultMatchIsSearch = newSuggestions.get(0).isSearchSuggestion();
            }
            if (mIsActive) {
                mDelegate.onSuggestionsChanged(inlineAutocompleteText, defaultMatchIsSearch);
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
        if (mNativeInitialized && mAutocomplete != null) {
            findMatchAndLoadUrl(urlText, eventTime, openInNewTab);
        } else {
            mDeferredLoadAction = () -> findMatchAndLoadUrl(urlText, eventTime, openInNewTab);
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
    private void findMatchAndLoadUrl(String urlText, long inputStart, boolean openInNewTab) {
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
            if (!mNativeInitialized || mAutocomplete == null) return;
            suggestionMatch = mAutocomplete.classify(urlText);
            // If urlText couldn't be classified, bail.
            if (suggestionMatch == null) return;
        }

        loadUrlForOmniboxMatch(
                0, suggestionMatch, suggestionMatch.getUrl(), inputStart, openInNewTab);
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
            boolean openInNewTab) {
        try (TraceEvent e = TraceEvent.scoped("AutocompleteMediator.loadUrlFromOmniboxMatch")) {
            OmniboxMetrics.recordFocusToOpenTime(System.currentTimeMillis() - mUrlFocusTime);

            // Clear the deferred site load action in case it executes. Reclaims a bit of memory.
            mDeferredLoadAction = null;

            mOmniboxFocusResultedInNavigation = true;
            url = updateSuggestionUrlIfNeeded(suggestion, matchIndex, url);

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

            // Kick off an action to clear focus and dismiss the suggestions list.
            // This normally happens when the target site loads and focus is moved to the
            // webcontents. On Android T we occasionally observe focus events to be lost, resulting
            // with Suggestions list obscuring the view.
            // TODO(crbug.com/1348324): clearing the Omnibox focus is slow, so we want to experiment
            // with two alternatives:
            // 1) Clear the Omnibox focus in a follow-up task. From a latency perspective, this is
            //    the best option: the navigation gets kicked off right away, and important
            //    navigation tasks can get scheduled between the current task and the task clearing
            //    the Omnibox focus. The ClearOmniboxFocusAfterNavigation feature with the
            //    clear_focus_asynchronously = false parameter (default) implements this option.
            // 2) Clear the Omnibox focus synchronously *after* the navigation has been kicked off.
            //    This allows some navigation work outside the browser process (e.g. running
            //    beforeunload handlers) to start ASAP. This is implemented by the setting the
            //    clear_focus_asynchronously = true parameter.
            if (!mClearFocusAfterNavigation) {
                finishInteraction();
            }

            if (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                mDelegate.loadUrlWithPostData(
                        url.getSpec(),
                        transition,
                        inputStart,
                        suggestion.getPostContentType(),
                        suggestion.getPostData());
            } else {
                mDelegate.loadUrl(url.getSpec(), transition, inputStart, openInNewTab);
            }

            if (mClearFocusAfterNavigationAsynchronously) {
                mHandler.post(mClearFocusCallback);
            } else if (mClearFocusAfterNavigation) {
                finishInteraction();
            }
        }
    }

    /** Sends a zero suggest request to the server in order to pre-populate the result cache. */
    /* package */ void startPrefetch() {
        int pageClassification =
                mDataProvider.getPageClassification(
                        /* isFocusedFromFakebox= */ false, /* isPrefetch= */ true);
        postAutocompleteRequest(
                () -> {
                    mAutocomplete.startPrefetch(mDataProvider.getCurrentGurl(), pageClassification);
                },
                SCHEDULE_FOR_IMMEDIATE_EXECUTION);
    }

    /**
     * Make a zero suggest request if: - The URL bar has focus. - The the tab/overview is not
     * incognito. This method should not be called directly. Schedule execution using
     * postAutocompleteRequest.
     */
    private void startZeroSuggest() {
        // Reset "edited" state in the omnibox if zero suggest is triggered -- new edits
        // now count as a new session.
        mEditSessionState = EditSessionState.INACTIVE;
        mNewOmniboxEditSessionTimestamp = -1;
        startMeasuringSuggestionRequestToUiModelTime();
        assert mNativeInitialized
                : "startZeroSuggest should be scheduled using postAutocompleteRequest";

        if (mDelegate.isUrlBarFocused()
                && (mDataProvider.hasTab() || mDataProvider.isInOverviewAndShowingOmnibox())) {
            int pageClassification =
                    mDataProvider.getPageClassification(
                            mDelegate.didFocusUrlFromFakebox(), /* isPrefetch= */ false);
            mShouldCacheSuggestions =
                    pageClassification == PageClassification.ANDROID_SEARCH_WIDGET_VALUE;
            mAutocomplete.startZeroSuggest(
                    mUrlBarEditingTextProvider.getTextWithAutocomplete(),
                    mDataProvider.getCurrentGurl(),
                    pageClassification,
                    mDataProvider.getTitle());
        }
    }

    /**
     * Update whether the omnibox suggestions are visible.
     *
     * @param shouldBeVisible whether the omnibox suggestions are visible
     */
    private void updateOmniboxSuggestionsVisibility(boolean shouldBeVisible) {
        boolean wasVisible = mListPropertyModel.get(SuggestionListProperties.VISIBLE);
        mListPropertyModel.set(SuggestionListProperties.VISIBLE, shouldBeVisible);
        if (shouldBeVisible && !wasVisible) {
            mIgnoreOmniboxItemSelection = true; // Reset to default value.
        }
    }

    /**
     * Hides the omnibox suggestion popup.
     *
     * <p>Signals the autocomplete controller to stop generating omnibox suggestions.
     *
     * @see AutocompleteController#stop(boolean)
     */
    private void hideSuggestions() {
        if (!mNativeInitialized || mAutocomplete == null) return;
        stopAutocomplete(true);
        dismissDeleteDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        mDropdownViewInfoListManager.clear();
        mAutocompleteResult = AutocompleteResult.EMPTY_RESULT;
    }

    /**
     * Signals the autocomplete controller to stop generating omnibox suggestions and cancels the
     * queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void stopAutocomplete(boolean clear) {
        if (mNativeInitialized && mAutocomplete != null) mAutocomplete.stop(clear);
        cancelAutocompleteRequests();
    }

    /** Trigger autocomplete for the given query. */
    void startAutocompleteForQuery(String query) {
        if (!mNativeInitialized || mAutocomplete == null) return;
        stopAutocomplete(false);
        if (mDataProvider.hasTab()) {
            mAutocomplete.start(
                    mDataProvider.getCurrentGurl(),
                    mDataProvider.getPageClassification(
                            /* isFocusedFromFakebox= */ false, /* isPrefetch= */ false),
                    query,
                    -1,
                    false);
        }
    }

    /**
     * Respond to Suggestion list height change and update list of presented suggestions.
     *
     * <p>This typically happens as a result of soft keyboard being shown or hidden.
     *
     * @param newHeightPx New height of the suggestion list in pixels.
     */
    public void onSuggestionDropdownHeightChanged(@Px int newHeight) {
        // Report the dropdown height whenever we intend to - or do show soft keyboard. This
        // addresses cases where hardware keyboard is attached to a device, or where user explicitly
        // called the keyboard back after we hid it.
        if (mDelegate.isKeyboardActive()) {
            mDropdownViewInfoListBuilder.setDropdownHeightWithKeyboardActive(newHeight);
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
    private void recordMetrics(AutocompleteMatch match, int suggestionLine, int disposition) {
        OmniboxMetrics.recordUsedSuggestionFromCache(mAutocompleteResult.isFromCachedResult());
        OmniboxMetrics.recordTouchDownPrefetchResult(match, mLastPrefetchStartedSuggestion);

        // Do not attempt to record other metrics for cached suggestions if the source of the list
        // is local cache. These suggestions do not have corresponding native objects and will fail
        // validation.
        if (mAutocompleteResult.isFromCachedResult()) return;

        GURL currentPageUrl = mDataProvider.getCurrentGurl();
        int pageClassification =
                mDataProvider.getPageClassification(
                        mDelegate.didFocusUrlFromFakebox(), /* isPrefetch= */ false);
        long elapsedTimeSinceModified = getElapsedTimeSinceInputChange();
        int autocompleteLength =
                mUrlBarEditingTextProvider.getTextWithAutocomplete().length()
                        - mUrlBarEditingTextProvider.getTextWithoutAutocomplete().length();
        WebContents webContents =
                mDataProvider.hasTab() ? mDataProvider.getTab().getWebContents() : null;

        mAutocomplete.onSuggestionSelected(
                match,
                suggestionLine,
                disposition,
                currentPageUrl,
                pageClassification,
                elapsedTimeSinceModified,
                autocompleteLength,
                webContents);
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
                new Runnable() {
                    @Override
                    public void run() {
                        mIsExecutingAutocompleteAction = true;
                        action.run();
                        mIsExecutingAutocompleteAction = false;
                        // Release completed Runnable.
                        mCurrentAutocompleteRequest = null;
                    }
                };
        // In the event we got Native Ready signal but no Profile yet (or the other way around),
        // delay execution of the Autocomplete request.
        if (!mNativeInitialized || mAutocomplete == null) return;
        if (delayMillis == SCHEDULE_FOR_IMMEDIATE_EXECUTION) {
            // TODO(crbug.com/1174855): Replace the following with postAtFrontOfQueue() and
            // correct any tests that expect data instantly.
            mCurrentAutocompleteRequest.run();
        } else {
            mHandler.postDelayed(mCurrentAutocompleteRequest, delayMillis);
        }
    }

    /** Cancel any pending autocomplete actions. */
    private void cancelAutocompleteRequests() {
        mShouldCacheSuggestions = false;
        stopMeasuringSuggestionRequestToUiModelTime();
        if (mCurrentAutocompleteRequest != null) {
            mHandler.removeCallbacks(mCurrentAutocompleteRequest);
            mCurrentAutocompleteRequest = null;
        }
    }

    /** Execute any pending Autocomplete requests, if the Autocomplete subsystem is ready. */
    private void runPendingAutocompleteRequests() {
        if (!mNativeInitialized || mAutocomplete == null) return;

        if (mDeferredLoadAction != null) {
            // Re-schedule the load action for execution.
            mHandler.post(mDeferredLoadAction);
            mDeferredLoadAction = null;
            cancelAutocompleteRequests();
        } else if (mCurrentAutocompleteRequest != null) {
            // Re-schedule the autocomplete action for immediate execution.
            // These requests are not executed until Native libraries are loaded.
            mHandler.postAtFrontOfQueue(mCurrentAutocompleteRequest);
        }
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
}
