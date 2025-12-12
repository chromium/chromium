// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier.NotifyBehavior;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.omnibox.DeferredIMEWindowInsetApplicationCallback;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics.RefineActionUsage;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionFactoryImpl;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteRequestType;
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

import java.util.List;
import java.util.function.Supplier;

/** Handles updating the model state for the currently visible omnibox suggestions. */
@NullMarked
class AutocompleteMediator
        implements OnSuggestionsReceivedListener,
                OmniboxSuggestionsDropdown.GestureObserver,
                OmniboxSuggestionsDropdownScrollListener,
                TopResumedActivityChangedObserver,
                PauseResumeWithNativeObserver,
                FuseboxAttachmentChangeListener,
                SuggestionHost {

    private static final int SCHEDULE_FOR_IMMEDIATE_EXECUTION = -1;

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;
    // Delay recording ZPS suppression to allow subsequent suggestion updates to arrive.
    private static final long ZPS_SUPPRESSION_METRIC_DEBOUNCE_MS = 100;

    private final Context mContext;
    private final AutocompleteDelegate mDelegate;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final PropertyModel mListPropertyModel;
    private final ModelList mSuggestionModels;
    private final Handler mHandler;
    private final LocationBarDataProvider mDataProvider;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final DropdownItemViewInfoListBuilder mDropdownViewInfoListBuilder;
    private final DropdownItemViewInfoListManager mDropdownViewInfoListManager;
    private final Callback<String> mBringTabGroupToFrontCallback;
    private final OmniboxActionDelegate mOmniboxActionDelegate;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final SuggestionsListAnimationDriver mAnimationDriver;
    private final WindowAndroid mWindowAndroid;
    private final DeferredIMEWindowInsetApplicationCallback
            mDeferredIMEWindowInsetApplicationCallback;
    private final OmniboxSuggestionsDropdownEmbedder mEmbedder;
    private final AutocompleteInput mAutocompleteInput = new AutocompleteInput();
    private final boolean mForcePhoneStyleOmnibox;
    private final Callback<@ControlsPosition Integer> mToolbarPositionChangedCallback =
            this::onToolbarPositionChanged;
    private final Callback<@AutocompleteRequestType Integer> mOnAutocompleteRequestTypeChanged =
            this::onAutocompleteRequestTypeChanged;

    private @Nullable AutocompleteController mAutocomplete;
    private @Nullable AutocompleteResult mAutocompleteResult;
    private @Nullable Runnable mCurrentAutocompleteRequest;
    private @Nullable Runnable mDeferredLoadAction;
    private @Nullable PropertyModel mDeleteDialogModel;

    private boolean mNativeInitialized;
    private long mUrlFocusTime;
    // When set, indicates if the omnibox is focused.
    private boolean mOmniboxFocused;
    // Tracks whether the activity window is currently focused.
    // This flag is updated via the onTopResumedActivityChanged(boolean) callback:
    // https://developer.android.com/reference/android/app/Activity#onTopResumedActivityChanged(boolean)
    // Default value is true, as this API is only available starting from API level 29.
    private boolean mActivityWindowFocused = true;
    // When set, specifies the system time of the most recent suggestion list request.
    private @Nullable Long mLastSuggestionRequestTime;
    // When set, specifies the time when the suggestion list was shown the first time.
    // Suggestions are refreshed several times per keystroke.
    private @Nullable Long mFirstSuggestionListModelCreatedTime;

    private @Nullable Boolean mOmniboxInZeroPrefixState;

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
    // The value of the last ZPS suppress metric recorded for the current ZPS session.
    // The value is reset to null for each new ZPS session.
    private @Nullable Boolean mLastRecordedZpsSuppressionValue;
    // Runnable to record the ZPS suppression metric. Used to debounce rapid updates.
    private @Nullable Runnable mRecordZpsSuppressionRunnable;

    /**
     * The text shown in the URL bar (user text + inline autocomplete) after the most recent set of
     * omnibox suggestions was received. When the user presses enter in the omnibox, this value is
     * compared to the URL bar text to determine whether the first suggestion is still valid.
     */
    private @Nullable String mUrlTextAfterSuggestionsReceived;

    private boolean mShouldPreventOmniboxAutocomplete;
    private long mLastActionUpTimestamp;
    private boolean mIgnoreOmniboxItemSelection = true;

    // The number of touch down events sent to native during an omnibox session.
    private int mNumTouchDownEventForwardedInOmniboxSession;
    // The number of prefetches that were started from touch down events during an omnibox session.
    private int mNumPrefetchesStartedInOmniboxSession;
    // The suggestion that the last prefetch was started for within the current omnibox session.
    private @Nullable AutocompleteMatch mLastPrefetchStartedSuggestion;

    // Observer watching for changes to the visual state of the omnibox suggestions.
    private @Nullable OmniboxSuggestionsVisualStateObserver mOmniboxSuggestionsVisualStateObserver;
    private final FuseboxCoordinator mFuseboxCoordinator;

    AutocompleteMediator(
            Context context,
            AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider,
            PropertyModel listPropertyModel,
            Handler handler,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            Supplier<@Nullable Tab> activityTabSupplier,
            @Nullable Supplier<ShareDelegate> shareDelegateSupplier,
            LocationBarDataProvider locationBarDataProvider,
            Callback<String> bringTabGroupToFrontCallback,
            BookmarkState bookmarkState,
            OmniboxActionDelegate omniboxActionDelegate,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            OmniboxSuggestionsDropdownEmbedder embedder,
            WindowAndroid windowAndroid,
            DeferredIMEWindowInsetApplicationCallback deferredIMEWindowInsetApplicationCallback,
            FuseboxCoordinator fuseboxCoordinator,
            boolean forcePhoneStyleOmnibox) {
        mContext = context;
        mDelegate = delegate;
        mUrlBarEditingTextProvider = textProvider;
        mListPropertyModel = listPropertyModel;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mHandler = handler;
        mDataProvider = locationBarDataProvider;
        mBringTabGroupToFrontCallback = bringTabGroupToFrontCallback;
        mFuseboxCoordinator = fuseboxCoordinator;
        mSuggestionModels = mListPropertyModel.get(SuggestionListProperties.SUGGESTION_MODELS);
        mOmniboxActionDelegate = omniboxActionDelegate;
        mWindowAndroid = windowAndroid;
        mEmbedder = embedder;
        mDropdownViewInfoListBuilder =
                new DropdownItemViewInfoListBuilder(
                        activityTabSupplier,
                        bookmarkState,
                        locationBarDataProvider.getToolbarPositionSupplier());
        mDropdownViewInfoListBuilder.setShareDelegateSupplier(shareDelegateSupplier);
        mDropdownViewInfoListManager =
                new DropdownItemViewInfoListManager(mSuggestionModels, context);
        OmniboxResourceProvider.invalidateDrawableCache();
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mDeferredIMEWindowInsetApplicationCallback = deferredIMEWindowInsetApplicationCallback;
        mForcePhoneStyleOmnibox = forcePhoneStyleOmnibox;

        var pm = context.getPackageManager();
        var dialIntent = new Intent(Intent.ACTION_DIAL);
        OmniboxActionFactoryImpl.get()
                .setDialerAvailable(!pm.queryIntentActivities(dialIntent, 0).isEmpty());

        mAnimationDriver = initializeAnimationDriver();

        mFuseboxCoordinator.addAttachmentChangeListener(this);
        mFuseboxCoordinator
                .getAutocompleteRequestTypeSupplier()
                .addSyncObserver(mOnAutocompleteRequestTypeChanged);

        mDataProvider
                .getToolbarPositionSupplier()
                .addObserver(mToolbarPositionChangedCallback, NotifyBehavior.NOTIFY_ON_ADD);
    }

    /**
     * Sets the observer watching the state of the omnibox suggestions. This observer will be
     * notifying of visual changes to the omnibox suggestions view, such as visibility or background
     * color changes.
     */
    void setOmniboxSuggestionsVisualStateObserver(
            @Nullable OmniboxSuggestionsVisualStateObserver omniboxSuggestionsVisualStateObserver) {
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
        mDataProvider.getToolbarPositionSupplier().removeObserver(mToolbarPositionChangedCallback);
        if (mAutocomplete != null) {
            mAutocomplete.removeOnSuggestionsReceivedListener(this);
        }

        if (mNativeInitialized) {
            OmniboxActionFactoryImpl.get().destroyNativeFactory();
        }
        mFuseboxCoordinator.removeAttachmentChangeListener(this);
        mFuseboxCoordinator
                .getAutocompleteRequestTypeSupplier()
                .removeObserver(mOnAutocompleteRequestTypeChanged);
        mHandler.removeCallbacksAndMessages(null);
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
        return (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE);
    }

    /**
     * Check if the suggestion is a link created from clipboard. It can be either a suggested
     * clipboard URL, or pasting an URL into the omnibox.
     *
     * @param suggestion The AutocompleteMatch to check.
     * @return Whether or not the suggestion is a link from clipboard.
     */
    private boolean isSuggestionLinkFromClipboard(AutocompleteMatch suggestion) {
        return (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || (suggestion.getType() == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                        && mUrlBarEditingTextProvider.wasLastEditPaste()));
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mAutocompleteResult != null ? mAutocompleteResult.getSuggestionsList().size() : 0;
    }

    /**
     * Retrieve the omnibox suggestion at the specified index. The index represents the ordering in
     * the underlying model. The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param matchIndex The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public @Nullable AutocompleteMatch getSuggestionAt(int matchIndex) {
        return mAutocompleteResult != null
                ? mAutocompleteResult.getSuggestionsList().get(matchIndex)
                : null;
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
        if (mOmniboxSuggestionsVisualStateObserver != null) {
            mOmniboxSuggestionsVisualStateObserver.onOmniboxSuggestionsBackgroundColorChanged(
                    OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                            mContext, brandedColorScheme));
        }
    }

    /**
     * Show cached zero suggest results. Enables Autocomplete subsystem to offer most recently
     * presented suggestions in the event where Native counterpart is not yet initialized.
     *
     * <p>Note: the only supported page context right now is the ANDROID_SEARCH_WIDGET.
     */
    void startCachedZeroSuggest() {
        boolean disableZps =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpNoZeroSuggest.getValue();

        // Do not show zero suggest results when omnibox autofocus is active on the Incognito NTP.
        // This suppresses all zero suggest requests before they are made, because it is unknown if
        // any zero suggest results would have been shown.
        if (disableZps && isOmniboxAutofocusOnIncognitoNtpActive()) {
            recordZeroSuggestSuppressionMetric(true);
            return;
        }

        maybeServeCachedResult();
        postAutocompleteRequest(this::startZeroSuggest, SCHEDULE_FOR_IMMEDIATE_EXECUTION);
    }

    /** Save AutocompleteResult to Cache for early serving. */
    private void maybeCacheResult(AutocompleteResult result) {
        if (!mAutocompleteInput.isInCacheableContext() || result.isFromCachedResult()) {
            return;
        }

        CachedZeroSuggestionsManager.saveToCache(
                mAutocompleteInput.getPageClassification(), result);
    }

    /** Serve AutocompleteResult from Cache if Autocomplete is not yet initialized. */
    private void maybeServeCachedResult() {
        if (!mAutocompleteInput.isInCacheableContext() || mAutocomplete != null) {
            return;
        }
        onSuggestionsReceived(
                CachedZeroSuggestionsManager.readFromCache(
                        mAutocompleteInput.getPageClassification()),
                true);
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
        if (mOmniboxFocused == activated) return;
        mOmniboxFocused = activated;

        // Propagate the information about omnibox session state change to all the processors first.
        // Processors need this for accounting purposes.
        // The change information should be passed before Processors receive first
        // batch of suggestions, that is:
        // - before any call to startZeroSuggest() (when first suggestions are populated), and
        // - before stopAutocomplete() (when current suggestions are erased).
        mDropdownViewInfoListBuilder.onOmniboxSessionStateChange(activated);

        if (mAnimationDriver.isAnimationEnabled()) {
            mAnimationDriver.onOmniboxSessionStateChange(activated);
            if (activated) {
                mDelegate.setKeyboardVisibility(true, false);
            }
        }

        if (activated) {
            initAutocompleteInput();

            // Do not attach IME observer when omnibox autofocus feature enabled and Incognito NTP
            // visible.
            if (!isOmniboxAutofocusOnIncognitoNtpActive()) {
                mDeferredIMEWindowInsetApplicationCallback.attach(mWindowAndroid);
            }

            dismissDeleteDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
            mRefineActionUsage = RefineActionUsage.NOT_USED;
            mOmniboxFocusResultedInNavigation = false;
            mSuggestionsListScrolled = false;
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
            mFuseboxCoordinator.notifyOmniboxSessionEnded(mOmniboxFocusResultedInNavigation);
            mDeferredIMEWindowInsetApplicationCallback.detach();
            stopMeasuringSuggestionRequestToUiModelTime();
            cancelAutocompleteRequests();
            OmniboxMetrics.recordOmniboxFocusResultedInNavigation(
                    mAutocompleteInput.getRequestType(),
                    mOmniboxFocusResultedInNavigation,
                    mFuseboxCoordinator.getAttachmentsCount() > 0);
            OmniboxMetrics.recordRefineActionUsage(mRefineActionUsage);
            OmniboxMetrics.recordSuggestionsListScrolled(
                    mAutocompleteInput.getPageClassification(), mSuggestionsListScrolled);

            // Reset the per omnibox session state of touch down prefetch.
            OmniboxMetrics.recordNumPrefetchesStartedInOmniboxSession(
                    mNumPrefetchesStartedInOmniboxSession);
            mNumTouchDownEventForwardedInOmniboxSession = 0;
            mNumPrefetchesStartedInOmniboxSession = 0;
            mLastPrefetchStartedSuggestion = null;

            mOmniboxInZeroPrefixState = null;
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
        if (hasFocus && mAnimationDriver.isAnimationEnabled()) {
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
        if (mAutocomplete != null) {
            mAutocomplete.removeOnSuggestionsReceivedListener(this);
        }
        mAutocomplete = AutocompleteController.getForProfile(profile);
        if (mAutocomplete != null) {
            mAutocomplete.addOnSuggestionsReceivedListener(this);
        }
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
        if (mAutocomplete != null) {
            mAutocomplete.onVoiceResults(results);
        }
    }

    /**
     * TODO(crbug.com/40725530): Figure out how to remove this.
     *
     * @return The current native pointer to the autocomplete results.
     */
    long getCurrentNativeAutocompleteResult() {
        return mAutocompleteResult != null ? mAutocompleteResult.getNativeObjectRef() : 0L;
    }

    /**
     * Triggered when the user selects one of the omnibox suggestions to navigate to.
     *
     * @param suggestion The AutocompleteMatch which was selected.
     * @param matchIndex Position of the suggestion in the drop down view.
     * @param url The URL associated with the suggestion.
     */
    @Override
    public void onSuggestionClicked(AutocompleteMatch suggestion, int matchIndex, GURL url) {
        // Android hub should always switch to tab if one is available.
        // TODO(crbug.com/369438026): Remove this block once switch-to-tab is the default action.
        boolean isAndroidHub =
                mAutocompleteInput.getPageClassification() == PageClassification.ANDROID_HUB_VALUE;
        if (isAndroidHub && suggestion.hasTabMatch()) {
            // Consider switching to tab for all other suggestion types that are not tab groups.
            if (suggestion.getType() == OmniboxSuggestionType.TAB_GROUP) {
                switchToTabGroup(suggestion);
                return;
            } else {
                var actions = suggestion.getActions();
                if (!actions.isEmpty()) {
                    var action = actions.get(0);
                    if (action instanceof OmniboxActionInSuggest omniboxActionInSuggest) {
                        if (mOmniboxActionDelegate.switchToTab(omniboxActionInSuggest.tabId, url)) {
                            // This bypasses the execution flow that captures histograms for all
                            // other cases.
                            recordMetrics(
                                    suggestion,
                                    null,
                                    matchIndex,
                                    WindowOpenDisposition.SWITCH_TO_TAB);
                            return;
                        }
                    }
                }
            }
        }

        mDeferredLoadAction =
                () ->
                        loadUrlForOmniboxMatch(
                                matchIndex,
                                suggestion,
                                url,
                                mLastActionUpTimestamp,
                                /* openInNewTab= */ false,
                                /* openInNewWindow= */ false);

        // Note: Action will be reset when load is initiated.
        if (mAutocomplete != null) {
            mDeferredLoadAction.run();
        }
    }

    /**
     * Triggered when the user touches down on a search suggestion.
     *
     * @param suggestion The AutocompleteMatch which was selected.
     * @param matchIndex Position of the suggestion in the drop down view.
     */
    @Override
    public void onSuggestionTouchDown(AutocompleteMatch suggestion, int matchIndex) {
        if (mAutocomplete == null
                || mNumTouchDownEventForwardedInOmniboxSession
                        >= OmniboxFeatures.getMaxPrefetchesPerOmniboxSession()) {
            return;
        }
        mNumTouchDownEventForwardedInOmniboxSession++;

        var tab = mDataProvider.getTab();
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        boolean wasPrefetchStarted =
                mAutocomplete != null
                        ? mAutocomplete.onSuggestionTouchDown(suggestion, matchIndex, webContents)
                        : false;
        if (wasPrefetchStarted) {
            mNumPrefetchesStartedInOmniboxSession++;
            mLastPrefetchStartedSuggestion = suggestion;
        }
    }

    @Override
    public void onOmniboxActionClicked(OmniboxAction action, int position) {
        var match = getSuggestionAt(position);
        if (match != null) {
            recordMetrics(match, action, position, action.disposition);
        }
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

    @VisibleForTesting
    public void switchToTabGroup(AutocompleteMatch match) {
        mBringTabGroupToFrontCallback.onResult(assumeNonNull(match.getTabGroupUuid()));
    }

    @Override
    public void onGesture(boolean isGestureUp, long timestamp) {
        stopAutocomplete(false);
        if (isGestureUp) {
            mLastActionUpTimestamp = timestamp;
        }
    }

    /**
     * Triggered when the user long presses the omnibox suggestion. A delete confirmation dialog
     * will be shown.
     *
     * @param suggestion The suggestion selected.
     * @param titleText The title to display in the delete dialog.
     */
    @Override
    public void confirmDeleteMatch(AutocompleteMatch suggestion, String titleText) {
        showDeleteDialog(
                suggestion,
                titleText,
                () -> {
                    RecordUserAction.record("MobileOmniboxRemoveSuggestion.LongPress");
                    deleteMatch(suggestion);
                });
    }

    /**
     * Triggered when the user clicks on the remove button to delete the suggestion immediately.
     *
     * @param suggestion The suggestion selected.
     */
    @Override
    public void deleteMatch(AutocompleteMatch suggestion) {
        if (mAutocomplete != null) {
            mAutocomplete.deleteMatch(suggestion);
        }
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
            AutocompleteMatch suggestion, String titleText, int elementIndex) {
        showDeleteDialog(
                suggestion,
                titleText,
                () -> {
                    if (mAutocomplete != null) {
                        mAutocomplete.deleteMatchElement(suggestion, elementIndex);
                    }
                });
    }

    /** Terminate the interaction with the Omnibox. */
    @Override
    public void finishInteraction() {
        mDelegate.clearOmniboxFocus();
        mAutocompleteInput.reset();
        mListPropertyModel.set(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE, false);
    }

    public void showDeleteDialog(
            AutocompleteMatch suggestion, String titleText, Runnable deleteAction) {
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
        var manager = mModalDialogManagerSupplier.get();
        if (mDeleteDialogModel != null) {
            assumeNonNull(manager).dismissDialog(mDeleteDialogModel, cause);
        }
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
     * URL to be of the corpus type if query in the omnibox is displayed and update gs_lcrp=
     * parameter on regular web search URLs.
     *
     * @param suggestion The chosen omnibox suggestion.
     * @param matchIndex The index of the chosen omnibox suggestion.
     * @param url The URL associated with the suggestion to navigate to.
     * @return The url to navigate to.
     */
    private GURL updateSuggestionUrlIfNeeded(AutocompleteMatch suggestion, GURL url) {
        if (mAutocomplete == null) return url;
        // TODO(crbug.com/40279214): this should exclude TILE variants when horizontal render group
        // is ready.
        if (suggestion.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST) {
            return url;
        }

        GURL updatedGurl =
                mAutocomplete.updateMatchDestinationUrlWithQueryFormulationTime(
                        suggestion, getElapsedTimeSinceInputChange());

        return updatedGurl != null ? updatedGurl : url;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     *
     * <p>The isOnFocusContext parameter signifies that the Zero Prefix Suggestions should be
     * retrieved even if the Omnibox content is not empty. This is relevant to Desktop mode Chrome,
     * where, if both physical keyboard and pointer device is attached, the Page URL should not be
     * cleared.
     *
     * @param textWithoutAutocomplete the text that does not include autocomplete information
     * @param isOnFocusContext whether Omnibox is currently gaining focus
     */
    public void onTextChanged(String textWithoutAutocomplete, boolean isOnFocusContext) {
        if (mShouldPreventOmniboxAutocomplete) return;

        // Always re-set the list's final state when we're about to request new suggestions.
        // This avoids a problem, where the property does not get an explicit update that the list
        // is final, which, in turn, may suppress certain functionality from getting invoked if the
        // subsequent push is immediately `final`.
        mListPropertyModel.set(SuggestionListProperties.LIST_IS_FINAL, false);

        mAutocompleteInput.setUserText(textWithoutAutocomplete);

        boolean isInZeroPrefixContext = mAutocompleteInput.isInZeroPrefixContext();
        mIgnoreOmniboxItemSelection = true;
        cancelAutocompleteRequests();

        // The user recently focused the Omnibox, began typing, or cleared the Omnibox.
        if (mOmniboxInZeroPrefixState == null
                || mOmniboxInZeroPrefixState != isInZeroPrefixContext) {
            mOmniboxInZeroPrefixState = isInZeroPrefixContext;
            if (!isInZeroPrefixContext) {
                // User started typing.
                if (mAutocomplete != null) {
                    mAutocomplete.resetSession();
                }
                mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
            } else {
                // Start a new ZPS session by resetting values.
                mLastRecordedZpsSuppressionValue = null;
                if (mRecordZpsSuppressionRunnable != null) {
                    mHandler.removeCallbacks(mRecordZpsSuppressionRunnable);
                    mRecordZpsSuppressionRunnable = null;
                }
            }
        }

        stopAutocomplete(false);

        if (isInZeroPrefixContext || isOnFocusContext) {
            clearSuggestions();
            startCachedZeroSuggest();
        } else {
            boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
            int cursorPosition =
                    mUrlBarEditingTextProvider.getSelectionStart()
                                    == mUrlBarEditingTextProvider.getSelectionEnd()
                            ? mUrlBarEditingTextProvider.getSelectionStart()
                            : -1;

            postAutocompleteRequest(
                    () -> {
                        startMeasuringSuggestionRequestToUiModelTime();
                        if (mAutocomplete != null) {
                            mAutocomplete.start(
                                    mAutocompleteInput, cursorPosition, preventAutocomplete);
                        }
                    },
                    OMNIBOX_SUGGESTION_START_DELAY_MS);
        }

        mDelegate.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsReceived(AutocompleteResult autocompleteResult, boolean isFinal) {
        // Persist AutocompleteResult in cache even if the interaction has just finished.
        // This allows us to cache most up-to-date information even after navigation was initiated.
        if (isFinal && !autocompleteResult.getSuggestionsList().isEmpty()) {
            maybeCacheResult(autocompleteResult);
        }

        // Reject results if the current session is inactive.
        if (!isActive()) return;

        @Nullable AutocompleteMatch defaultMatch = autocompleteResult.getDefaultMatch();
        String inlineAutocompleteText =
                defaultMatch != null ? defaultMatch.getInlineAutocompletion() : "";

        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        if (!(mAutocompleteResult != null && mAutocompleteResult.equals(autocompleteResult))) {
            mAutocompleteResult = autocompleteResult;
            var viewInfoList =
                    mDropdownViewInfoListBuilder.buildDropdownViewInfoList(
                            mAutocompleteInput, autocompleteResult);
            mDropdownViewInfoListManager.setSourceViewInfoList(viewInfoList);
            mDelegate.onSuggestionsChanged(defaultMatch);
        }

        mListPropertyModel.set(SuggestionListProperties.LIST_IS_FINAL, isFinal);
        measureSuggestionRequestToUiModelTime(isFinal);
    }

    public void onAutocompleteRequestTypeChanged(@AutocompleteRequestType int type) {
        if (mOmniboxFocused) {
            mAutocompleteInput.setRequestType(type);
            mAutocompleteInput.setPageClassification(mDataProvider.getPageClassification(false));
            onTextChanged(
                    mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                    /* isOnFocusContext= */ false);
        }
    }

    /**
     * Load the url corresponding to the typed omnibox text.
     *
     * @param eventTime The timestamp the load was triggered by the user.
     * @param openInNewTab Whether the URL will be loaded in a new tab. If {@code true}, the URL
     *     will be loaded in a new tab. If {@code false}, The URL will be loaded in the current tab.
     * @param openInNewWindow Whether the URL will be loaded in a new window. If {@code true}, the
     *     URL will be loaded in a new window. If {@code false}, The URL will be loaded in the
     *     current window.
     */
    void loadTypedOmniboxText(long eventTime, boolean openInNewTab, boolean openInNewWindow) {
        assert !openInNewTab || !openInNewWindow
                : "Unable to determine if the URL should be loaded in a new tab in the current"
                        + " window or in a new window.";

        final String urlText = mUrlBarEditingTextProvider.getTextWithAutocomplete();
        cancelAutocompleteRequests();

        if (mAutocompleteInput.getPageClassification() == PageClassification.ANDROID_HUB_VALUE) {
            RecordUserAction.record("HubSearch.KeyboardEnterPressed");
            // For Hub Search, default behavior kicks off search by pressing enter, do not return.
        }

        if (mAutocomplete != null) {
            findMatchAndLoadUrl(urlText, eventTime, openInNewTab, openInNewWindow);
        } else {
            mDeferredLoadAction =
                    () -> findMatchAndLoadUrl(urlText, eventTime, openInNewTab, openInNewWindow);
        }
    }

    /**
     * Search for a suggestion with the same associated URL as the supplied one.
     *
     * @param urlText The URL text to search for.
     * @param inputStart The timestamp the load was triggered by the user.
     * @param openInNewTab Whether the URL will be loaded in a new tab. If {@code true}, the URL
     *     will be loaded in a new tab. If {@code false}, The URL will be loaded in the current tab.
     * @param openInNewWindow Whether the URL will be loaded in a new window. If {@code true}, the
     *     URL will be loaded in a new window. If {@code false}, The URL will be loaded in the
     *     current window.
     */
    private void findMatchAndLoadUrl(
            String urlText, long inputStart, boolean openInNewTab, boolean openInNewWindow) {
        AutocompleteMatch suggestionMatch = getSuggestionMatchForUrlText(urlText);

        if (suggestionMatch == null) return;
        loadUrlForOmniboxMatch(
                0,
                suggestionMatch,
                suggestionMatch.getUrl(),
                inputStart,
                openInNewTab,
                openInNewWindow);
    }

    private @Nullable AutocompleteMatch getSuggestionMatchForUrlText(String urlText) {
        if (getSuggestionCount() > 0
                && mUrlTextAfterSuggestionsReceived != null
                && urlText.trim().equals(mUrlTextAfterSuggestionsReceived.trim())) {
            // Common case: the user typed something, received suggestions, then pressed enter.
            // This triggers the Default Match.
            return getSuggestionAt(0);
        } else {
            // Less common case: there are no valid omnibox suggestions. This can happen if the
            // user tapped the URL bar to dismiss the suggestions, then pressed enter. This can
            // also happen if the user presses enter before any suggestions have been received
            // from the autocomplete controller.
            return mAutocomplete != null ? mAutocomplete.classify(urlText) : null;
            // If urlText couldn't be classified, bail.
        }
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
     * @param openInNewWindow Whether the URL will be loaded in a new window. If {@code true}, the
     *     URL will be loaded in a new window. If {@code false}, The URL will be loaded in the
     *     current window.
     */
    private void loadUrlForOmniboxMatch(
            int matchIndex,
            AutocompleteMatch suggestion,
            GURL url,
            long inputStart,
            boolean openInNewTab,
            boolean openInNewWindow) {
        try (TraceEvent e = TraceEvent.scoped("AutocompleteMediator.loadUrlFromOmniboxMatch")) {
            OmniboxMetrics.recordFocusToOpenTime(System.currentTimeMillis() - mUrlFocusTime);

            // Clear the deferred site load action in case it executes. Reclaims a bit of memory.
            mDeferredLoadAction = null;

            mOmniboxFocusResultedInNavigation = true;

            url = updateSuggestionUrlIfNeeded(suggestion, url);

            url =
                    switch (mFuseboxCoordinator.getAutocompleteRequestTypeSupplier().get()) {
                        case AutocompleteRequestType.AI_MODE -> mFuseboxCoordinator.getAimUrl(url);
                        case AutocompleteRequestType.IMAGE_GENERATION ->
                                mFuseboxCoordinator.getImageGenerationUrl(url);
                        default -> url;
                    };

            // loadUrl modifies AutocompleteController's state clearing the native
            // AutocompleteResults needed by onSuggestionsSelected. Therefore,
            // loadUrl should should be invoked last.
            int transition = suggestion.getTransition();
            int type = suggestion.getType();

            recordMetrics(suggestion, null, matchIndex, WindowOpenDisposition.CURRENT_TAB);
            if (type == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
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
                                if (mAutocomplete != null) {
                                    mAutocomplete.createNavigationObserver(
                                            loadUrlResult.navigationHandle, suggestion);
                                }
                            }
                        }
                    };

            mDelegate.loadUrl(
                    new OmniboxLoadUrlParams.Builder(url.getSpec(), transition)
                            .setInputStartTimestamp(inputStart)
                            .setPostData(suggestion.getPostData())
                            .setOpenInNewTab(openInNewTab)
                            .setOpenInNewWindow(openInNewWindow)
                            .setExtraHeaders(suggestion.getExtraHeaders())
                            .setAutocompleteLoadCallback(autocompleteLoadCallback)
                            .build());

            mHandler.post(this::finishInteraction);
        }
    }

    /**
     * Sends a zero suggest request to the server in order to pre-populate the result cache.
     *
     * @param webContents The WebContents for the current tab.
     */
    /* package */ void startPrefetch(@Nullable WebContents webContents) {
        postAutocompleteRequest(
                () -> {
                    if (mAutocomplete != null) {
                        final AutocompleteInput input = new AutocompleteInput();
                        input.setPageClassification(mDataProvider.getPageClassification(true));
                        input.setPageUrl(mDataProvider.getCurrentGurl());
                        input.setPageTitle(mDataProvider.getTitle());
                        mAutocomplete.startPrefetch(input, webContents);
                    }
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
        mNewOmniboxEditSessionTimestamp = -1;
        startMeasuringSuggestionRequestToUiModelTime();

        if (mDelegate.isUrlBarFocused()) {
            if (mAutocomplete != null) {
                mAutocomplete.startZeroSuggest(mAutocompleteInput);
            }
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
            if (mOmniboxSuggestionsVisualStateObserver != null) {
                mOmniboxSuggestionsVisualStateObserver.onOmniboxSessionStateChange(isActive);
            }
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
        mAutocompleteResult = null;
    }

    /**
     * Signals the autocomplete controller to stop generating omnibox suggestions and cancels the
     * queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    @VisibleForTesting
    void stopAutocomplete(boolean clear) {
        if (mAutocomplete != null) {
            mAutocomplete.stop(clear);
        }
        // All suggestions are now removed.
        cancelAutocompleteRequests();
    }

    /**
     * Initialize the AutocompleteInput with the data from the active tab. This method is invoked
     * every time the new Omnibox session is started.
     */
    @VisibleForTesting
    void initAutocompleteInput() {
        mAutocompleteInput.setPageClassification(
                mDataProvider.getPageClassification(/* prefetch= */ false));
        mAutocompleteInput.setRequestType(
                mFuseboxCoordinator.getAutocompleteRequestTypeSupplier().get());
        mAutocompleteInput.setPageUrl(mDataProvider.getCurrentGurl());
        mAutocompleteInput.setPageTitle(mDataProvider.getTitle());

        mListPropertyModel.set(
                SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE,
                mAutocompleteInput.getPageClassification() == PageClassification.ANDROID_HUB_VALUE);
        mListPropertyModel.set(
                SuggestionListProperties.IS_LARGE_SCREEN,
                !mForcePhoneStyleOmnibox
                        && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                        && mContext.getResources().getConfiguration().screenWidthDp
                                >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP);
    }

    /** Trigger autocomplete for the given query. */
    void startAutocompleteForQuery(String query) {
        stopAutocomplete(false);
        initAutocompleteInput();
        mAutocompleteInput.setUserText(query);
        if (mAutocomplete != null) {
            mAutocomplete.start(mAutocompleteInput, -1, false);
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
            if (mAutocomplete != null) {
                mAutocomplete.onSuggestionDropdownHeightChanged(newHeight, suggestionHeight);
            }
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
     * @param action the OmniboxAction associated with the suggestion
     */
    private void recordMetrics(
            AutocompleteMatch match,
            @Nullable OmniboxAction action,
            int suggestionLine,
            int disposition) {
        if (mAutocompleteResult == null) return;

        boolean autocompleteResultIsFromCache =
                mAutocompleteResult != null ? mAutocompleteResult.isFromCachedResult() : true;

        OmniboxMetrics.recordUsedSuggestionFromCache(autocompleteResultIsFromCache);
        OmniboxMetrics.recordTouchDownPrefetchResult(match, mLastPrefetchStartedSuggestion);

        // Do not attempt to record other metrics for cached suggestions if the source of the list
        // is local cache. These suggestions do not have corresponding native objects and will fail
        // validation.
        if (autocompleteResultIsFromCache) return;

        GURL currentPageUrl = mAutocompleteInput.getPageUrl();
        long elapsedTimeSinceModified = getElapsedTimeSinceInputChange();
        int autocompleteLength =
                mUrlBarEditingTextProvider.getTextWithAutocomplete().length()
                        - mUrlBarEditingTextProvider.getTextWithoutAutocomplete().length();
        var tab = mDataProvider.getTab();
        WebContents webContents = tab != null ? tab.getWebContents() : null;

        if (mAutocomplete != null) {
            mAutocomplete.onSuggestionSelected(
                    match,
                    suggestionLine,
                    disposition,
                    currentPageUrl,
                    mAutocompleteInput.getPageClassification(),
                    elapsedTimeSinceModified,
                    autocompleteLength,
                    webContents,
                    action);
        }
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
    private void postAutocompleteRequest(Runnable action, long delayMillis) {
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
        if (mCurrentAutocompleteRequest != null) {
            if (delayMillis == SCHEDULE_FOR_IMMEDIATE_EXECUTION) {
                // TODO(crbug.com/40167699): Replace the following with postAtFrontOfQueue()
                // and correct any tests that expect data instantly.
                mCurrentAutocompleteRequest.run();
            } else {
                mHandler.postDelayed(mCurrentAutocompleteRequest, delayMillis);
            }
        }
    }

    /** Cancel any pending autocomplete actions. */
    private void cancelAutocompleteRequests() {
        stopMeasuringSuggestionRequestToUiModelTime();
        if (mCurrentAutocompleteRequest != null) {
            mHandler.removeCallbacks(mCurrentAutocompleteRequest);
        }
        mCurrentAutocompleteRequest = null;
    }

    /** Execute any pending Autocomplete requests, if the Autocomplete subsystem is ready. */
    private void runPendingAutocompleteRequests() {
        if (!mNativeInitialized || mAutocomplete == null) return;

        // Set the Page URL now. This is a corner case for the SearchActivity, which is unable to
        // resolve the Page URL until Profile is available.
        // All other scenarios should rely on `onOmniboxSessionStateChange()` handling.
        initAutocompleteInput();

        if (mDeferredLoadAction != null) {
            // If deferred load action is present, cancel all autocomplete and load the URL.
            cancelAutocompleteRequests();
            mHandler.postAtFrontOfQueue(mDeferredLoadAction);
        } else if (mCurrentAutocompleteRequest != null) {
            // Otherwise, run pending autocomplete action (if any).
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

    @VisibleForTesting
    SuggestionsListAnimationDriver initializeAnimationDriver() {
        SuggestionsListAnimationDriver driver;
        if (mDelegate.isToolbarPositionCustomizationEnabled()
                || OmniboxFeatures.shouldAnimateSuggestionsListAppearance()) {
            driver =
                    new UnsyncedSuggestionsListAnimationDriver(
                            mListPropertyModel,
                            () -> propagateOmniboxSessionStateChange(true),
                            mDelegate::isToolbarBottomAnchored,
                            mEmbedder::getVerticalTranslationForAnimation,
                            mContext);
        } else {
            driver =
                    new SuggestionsListAnimationDriver() {
                        @Override
                        public void onOmniboxSessionStateChange(boolean active) {}

                        @Override
                        public boolean isAnimationEnabled() {
                            return false;
                        }
                    };
        }
        return driver;
    }

    private void onToolbarPositionChanged(@ControlsPosition Integer newPosition) {
        mListPropertyModel.set(SuggestionListProperties.TOOLBAR_POSITION, newPosition);
        if (isActive()) {
            // Hacky solution: rebuild the list if we're active when the position changes,
            // triggering recalculation of refine arrow icon. TODO(http://crbug.com/446058347):
            // refactor to enable updates to the icon property of the model once the list is already
            // built.
            onTextChanged(
                    mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                    /* isOnFocusContext= */ false);
        }
    }

    /** Returns the current AutocompleteInput instance. */
    AutocompleteInput getAutocompleteInputForTesting() {
        return mAutocompleteInput;
    }

    /** Returns whether Omnibox session is active (the user is interacting with the Omnibox). */
    boolean isOmniboxSessionActiveForTesting() {
        return mOmniboxFocused;
    }

    /** Returns the current Animation Driver instance. */
    SuggestionsListAnimationDriver getAnimationDriverForTesting() {
        return mAnimationDriver;
    }

    /**
     * @see FuseboxAttachmentChangeListener#onAttachmentListChanged()
     */
    @Override
    public void onAttachmentListChanged() {
        if (!isActive()) return;

        // Re-request ZPS in the event of attachments being removed/replaced.
        onTextChanged(
                mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                /* isOnFocusContext= */ false);
    }

    /**
     * @see FuseboxAttachmentChangeListener#onAttachmentUploadStatusChanged()
     */
    @Override
    public void onAttachmentUploadStatusChanged() {
        if (!isActive()) return;

        // Re-request ZPS in the event of new attachments being uploaded.
        onTextChanged(
                mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                /* isOnFocusContext= */ false);
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        mActivityWindowFocused = isTopResumedActivity;
        // Always set the window activity focused property to true for hub search so that the
        // dropdown container persists when search activity is dismissed.
        // TODO(crbug.com/390011136): Find a better way to create a seamless animation when
        // exiting hub search that dismisses the URL bar and suggestions list together.
        mListPropertyModel.set(
                SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED,
                mAutocompleteInput.getPageClassification() == PageClassification.ANDROID_HUB_VALUE
                        ? true
                        : isTopResumedActivity);
        if (isActive()) {
            onTextChanged(
                    mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                    /* isOnFocusContext= */ false);
        }
    }

    private boolean isActive() {
        return mOmniboxFocused && mActivityWindowFocused;
    }

    @Override
    public void onResumeWithNative() {}

    @Override
    public void onPauseWithNative() {
        // IMPORTANT:
        // Test builds often mock AutocompleteController. This mock object may be defunct when we
        // this code is reached. Do not execute this code as part of integration tests as it will
        // attempt to interact with dead mocks.
        if (BuildConfig.IS_FOR_TEST) return;

        // Detect the window focus has changed. This may be due to the user entering app switcher,
        // pressing the home screen, or, in windowed/split screen mode, user interacting with a
        // different app. This gives us enough head room to retrieve and cache relevant information.
        // Note: onPause and onUserLeaveHint happen much too late.
        if (!OmniboxFeatures.isJumpStartOmniboxEnabled()) return;

        // Abort early if Autocomplete has not initialized yet.
        if (mAutocomplete == null) return;

        // Default page context to prefetch suggestions for.
        GURL pageUrl = UrlConstants.ntpGurl();
        int pageClass = PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE;

        // Preserve current page context for Jump-start Omnibox feature.
        if (OmniboxFeatures.sJumpStartOmniboxCoverRecentlyVisitedPage.getValue()) {
            pageUrl = mAutocompleteInput.getPageUrl();
            pageClass = mAutocompleteInput.getPageClassification();

            var currentContext = CachedZeroSuggestionsManager.readJumpStartContext();
            if (currentContext.pageClass == pageClass && currentContext.url.equals(pageUrl)) {
                return;
            }

            // The context has changed. Avoid showing stale suggestions.
            CachedZeroSuggestionsManager.saveJumpStartContext(
                    new CachedZeroSuggestionsManager.JumpStartContext(pageUrl, pageClass));
            CachedZeroSuggestionsManager.eraseCachedSuggestionsByPageClass(pageClass);
        }
        var input = new AutocompleteInput();
        input.setPageUrl(pageUrl);
        input.setPageClassification(pageClass);

        // Retrieve suggestions related to the most recently visited page.
        // This is a best-effort action and may not always work (e.g. if Chrome gets killed or
        // swiped away before we manage to retrieve and persist the information).
        mAutocomplete.startZeroSuggest(input);
    }

    /**
     * Returns whether the Omnibox Autofocus on Incognito NTP feature is enabled and the Incognito
     * NTP is currently visible.
     *
     * @return True if the feature is enabled and Incognito NTP is visible, false otherwise.
     */
    private boolean isOmniboxAutofocusOnIncognitoNtpActive() {
        return ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtp.isEnabled()
                && mDataProvider.getNewTabPageDelegate().isIncognitoNewTabPageCurrentlyVisible();
    }

    /**
     * Records metric about zero-prefix suggestions suppression on the Incognito NTP.
     *
     * @param suppressed Whether zero-prefix suggestions were suppressed.
     */
    private void recordZeroSuggestSuppressionMetric(boolean suppressed) {
        // Do not record if a metric has already been recorded for current ZPS session.
        if (mLastRecordedZpsSuppressionValue != null) {
            return;
        }

        // Cancel any pending recording to reset the debounce timer.
        if (mRecordZpsSuppressionRunnable != null) {
            mHandler.removeCallbacks(mRecordZpsSuppressionRunnable);
        }

        mRecordZpsSuppressionRunnable =
                () -> {
                    OmniboxMetrics.recordZeroSuggestSuppressedOnIncognitoNtp(suppressed);
                    mLastRecordedZpsSuppressionValue = suppressed;
                    mRecordZpsSuppressionRunnable = null;
                };

        mHandler.postDelayed(mRecordZpsSuppressionRunnable, ZPS_SUPPRESSION_METRIC_DEBOUNCE_MS);
    }
}
