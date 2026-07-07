// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.SearchEngineService.SearchEngineNameObserver;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.url.GURL;

/**
 * Handles tracking updates and calculating what should be used for the omnibox hint text. Accepts a
 * callback to push updated text back to client.
 */
@NullMarked
public class HintTextUpdater implements LocationBarDataProvider.Observer {
    private final Context mContext;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final LocationBarEmbedderUiOverrides mEmbedderUiOverrides;
    private final Callback<String> mUpdateHintTextCallback;
    private final MonotonicObservableSupplier<SearchEngineService> mSearchEngineServiceSupplier;
    private final FuseboxCoordinator mFuseboxCoordinator;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final SearchEngineNameObserver mSearchEngineNameObserver = this::updateHintText;
    private final Callback<@AutocompleteRequestType Integer> mAutocompleteRequestTypeObserver =
            (type) -> updateHintText();
    private final Callback<@Nullable SiteSearchData> mSiteSearchDataObserver =
            (siteSearchData) -> updateHintText();
    private final Callback<SearchEngineService> mSearchEngineServiceObserver =
            this::onSearchEngineServiceChanged;
    private final Callback<@FuseboxState Integer> mFuseboxStateObserver =
            (state) -> updateHintText();
    private final Callback<@FuseboxLayoutMode Integer> mFuseboxLayoutModeObserver =
            (mode) -> updateHintText();
    private final Callback<Boolean> mActivationChipVisibilityObserver =
            (visible) -> updateHintText();
    private final Callback<Profile> mProfileObserver = (profile) -> updateHintText();
    private final Callback<String> mUserTextObserver = (text) -> updateHintText();

    private @Nullable SearchEngineService mSearchEngineService;
    private @Nullable AutocompleteInput mCurrentInput;
    private boolean mAimHintShownThisSession;

    public HintTextUpdater(
            Context context,
            LocationBarDataProvider locationBarDataProvider,
            LocationBarEmbedderUiOverrides embedderUiOverrides,
            MonotonicObservableSupplier<SearchEngineService> searchEngineServiceSupplier,
            FuseboxCoordinator fuseboxCoordinator,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Callback<String> updateHintTextCallback) {
        mContext = context;
        mLocationBarDataProvider = locationBarDataProvider;
        mEmbedderUiOverrides = embedderUiOverrides;
        mSearchEngineServiceSupplier = searchEngineServiceSupplier;
        mFuseboxCoordinator = fuseboxCoordinator;
        mProfileSupplier = profileSupplier;
        mUpdateHintTextCallback = updateHintTextCallback;
        mLocationBarDataProvider.addObserver(this);

        mSearchEngineServiceSupplier.addSyncObserver(mSearchEngineServiceObserver);
        mFuseboxCoordinator.getFuseboxStateSupplier().addSyncObserver(mFuseboxStateObserver);
        mFuseboxCoordinator
                .getFuseboxLayoutModeSupplier()
                .addSyncObserver(mFuseboxLayoutModeObserver);
        mFuseboxCoordinator
                .getActivationChipVisibilitySupplier()
                .addSyncObserver(mActivationChipVisibilityObserver);
        mProfileSupplier.addSyncObserver(mProfileObserver);

        updateHintText();
    }

    /** Clean up resources used by this class. */
    public void destroy() {
        mLocationBarDataProvider.removeObserver(this);
        mSearchEngineServiceSupplier.removeObserver(mSearchEngineServiceObserver);
        if (mSearchEngineService != null) {
            mSearchEngineService.removeSearchEngineNameObserver(mSearchEngineNameObserver);
        }
        mFuseboxCoordinator.getFuseboxStateSupplier().removeObserver(mFuseboxStateObserver);
        mFuseboxCoordinator
                .getFuseboxLayoutModeSupplier()
                .removeObserver(mFuseboxLayoutModeObserver);
        mFuseboxCoordinator
                .getActivationChipVisibilitySupplier()
                .removeObserver(mActivationChipVisibilityObserver);
        mProfileSupplier.removeObserver(mProfileObserver);
        endInput();
    }


    /**
     * Begins a new input session.
     *
     * @param input The input for the current session.
     */
    public void beginInput(AutocompleteInput input) {
        if (mCurrentInput != null) {
            endInput();
        }
        mCurrentInput = input;
        mCurrentInput.getRequestTypeSupplier().addSyncObserver(mAutocompleteRequestTypeObserver);
        mCurrentInput.getSiteSearchDataSupplier().addSyncObserver(mSiteSearchDataObserver);
        mCurrentInput.getUserTextSupplier().addSyncObserver(mUserTextObserver);
        updateHintText();
    }

    /** Ends the current input session. */
    public void endInput() {
        if (mCurrentInput != null) {
            mCurrentInput.getRequestTypeSupplier().removeObserver(mAutocompleteRequestTypeObserver);
            mCurrentInput.getSiteSearchDataSupplier().removeObserver(mSiteSearchDataObserver);
            mCurrentInput.getUserTextSupplier().removeObserver(mUserTextObserver);
        }
        dismissAimHintIph();
        mCurrentInput = null;
        updateHintText();
    }

    @Override
    public void onTitleChanged() {
        updateHintText();
    }

    private void onSearchEngineServiceChanged(@Nullable SearchEngineService service) {
        if (mSearchEngineService != null) {
            mSearchEngineService.removeSearchEngineNameObserver(mSearchEngineNameObserver);
        }
        mSearchEngineService = service;
        if (mSearchEngineService != null) {
            mSearchEngineService.addSearchEngineNameObserver(mSearchEngineNameObserver);
        }
        updateHintText();
    }

    private void updateHintText() {
        if (mSearchEngineService == null) return;
        if (mEmbedderUiOverrides.isEmbedderControlledHint()) return;

        if (mCurrentInput != null && mCurrentInput.getSiteSearchData() != null) {
            mUpdateHintTextCallback.onResult("");
            return;
        }

        @AutocompleteRequestType
        int requestType =
                mCurrentInput == null
                        ? mLocationBarDataProvider.getDefaultRequestType()
                        : mCurrentInput.getRequestType();

        if (useAimActivationOrEmptyHint()) {
            if (triggerOrAlreadyShowingActivationHint()) {
                mUpdateHintTextCallback.onResult(
                        mContext.getString(R.string.acc_ai_mode_placeholder_text));
            } else {
                mUpdateHintTextCallback.onResult("");
            }
            return;
        }

        FuseboxSessionState fuseboxSession = FuseboxSessionState.from(mLocationBarDataProvider);
        String hint = getOmniboxHintText(requestType, fuseboxSession);
        mUpdateHintTextCallback.onResult(hint);
    }

    private String getOmniboxHintText(
            @AutocompleteRequestType int requestType,
            @Nullable FuseboxSessionState fuseboxSessionState) {
        if (fuseboxSessionState != null) {
            var input = fuseboxSessionState.getAutocompleteInput();
            if (input != null) {
                GURL url = input.getPageUrl();
                String title = input.getPageTitle();
                if (ContextualTasksUtils.isContextualTasksUrl(url) && !TextUtils.isEmpty(title)) {
                    return title;
                }
            }
        }

        assert mSearchEngineService != null;
        String searchEngineName = mSearchEngineService.getSearchEngineName();
        if (TextUtils.isEmpty(searchEngineName)) {
            return OmniboxResourceProvider.getString(mContext, R.string.omnibox_empty_hint);
        }

        if (OmniboxFeatures.sShowModelPicker.getValue()
                && ToolModeUtils.isAimRequest(requestType)) {
            String toolHint = getToolHintFromInputState(requestType, fuseboxSessionState);
            if (!TextUtils.isEmpty(toolHint)) {
                return toolHint;
            }
        }

        switch (requestType) {
            case AutocompleteRequestType.AI_MODE:
                return OmniboxResourceProvider.getString(
                        mContext,
                        R.string.omnibox_ai_mode_scope_placeholder_text,
                        searchEngineName);
            case AutocompleteRequestType.IMAGE_GENERATION:
                return OmniboxResourceProvider.getString(
                        mContext,
                        R.string.omnibox_empty_hint_for_image_generation,
                        searchEngineName);
        }
        return mSearchEngineService.getOmniboxHintString();
    }

    private @Nullable String getToolHintFromInputState(
            @AutocompleteRequestType int requestType,
            @Nullable FuseboxSessionState fuseboxSessionState) {
        if (fuseboxSessionState == null) return null;

        ComposeboxQueryControllerBridge bridge =
                fuseboxSessionState.getComposeboxQueryControllerBridge();
        if (bridge == null) return null;

        InputState inputState = bridge.getInputStateSupplier().get();
        if (inputState == null) return null;

        int activeTool =
                ToolModeUtils.getToolModeForRequestType(requestType, /* hasAttachments= */ false);
        for (ToolConfig config : inputState.toolConfigs) {
            if (config.getToolValue() == activeTool) {
                return config.getHintText();
            }
        }

        return null;
    }

    private boolean useAimActivationOrEmptyHint() {
        return mFuseboxCoordinator.getFuseboxStateSupplier().get() != FuseboxState.DISABLED
                && isSuggestionsPopover()
                && mFuseboxCoordinator.getActivationChipVisibilitySupplier().get();
    }

    private boolean isSuggestionsPopover() {
        return mFuseboxCoordinator.getFuseboxLayoutModeSupplier().get()
                == FuseboxLayoutMode.SUGGESTIONS_POPOVER;
    }

    private boolean triggerOrAlreadyShowingActivationHint() {
        boolean isFullyFocused =
                mCurrentInput != null
                        && mCurrentInput.getAutocompleteState() == AutocompleteState.ENABLED;
        boolean isUrlBarEmpty =
                mCurrentInput != null && TextUtils.isEmpty(mCurrentInput.getUserText());
        Profile profile = mProfileSupplier.get();
        Tracker tracker = profile != null ? TrackerFactory.getTrackerForProfile(profile) : null;

        if (tracker != null && isFullyFocused && isUrlBarEmpty) {
            if (mAimHintShownThisSession) {
                return true;
            } else {
                mAimHintShownThisSession =
                        tracker.shouldTriggerHelpUi(FeatureConstants.AIM_ACTIVATION_HINT);
                return mAimHintShownThisSession;
            }
        }

        return false;
    }

    private void dismissAimHintIph() {
        if (mAimHintShownThisSession) {
            Profile profile = mProfileSupplier.get();
            Tracker tracker = profile != null ? TrackerFactory.getTrackerForProfile(profile) : null;
            if (tracker != null) {
                tracker.dismissed(FeatureConstants.AIM_ACTIVATION_HINT);
            }
            mAimHintShownThisSession = false;
        }
    }
}
