// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler.VoiceResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Class containing functionality related to voice search. */
@NullMarked
public class VoiceRecognitionHandler {
    private static final String TAG = "VoiceRecognition";

    /**
     * The minimum confidence threshold that will result in navigating directly to a voice search
     * response (as opposed to treating it like a typed string in the Omnibox).
     */
    @VisibleForTesting public static final float VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD = 0.9f;

    private final OmniboxStub mOmniboxStub;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final AutocompleteCoordinator mAutocompleteCoordinator;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private @Nullable WebContentsObserver mVoiceSearchWebContentsObserver;
    private CallbackController mCallbackController = new CallbackController();
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final VoiceRecognitionIntentHandler mIntentHandler;

    /**
     * AssistantActionPerformed defined in tools/metrics/histograms/enums.xml.
     *
     * <p>Do not reorder or remove items, only add new items before NUM_ENTRIES.
     */
    @IntDef({
        AssistantActionPerformed.UNKNOWN,
        AssistantActionPerformed.TRANSCRIPTION,
        AssistantActionPerformed.TRANSLATE,
        AssistantActionPerformed.READOUT
    })
    public @interface AssistantActionPerformed {
        int UNKNOWN = 0;
        int TRANSCRIPTION = 1;
        int TRANSLATE = 2;
        int READOUT = 3;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    /** Interface for observers interested in updates to the voice state. */
    public interface Observer {
        /**
         * Triggers when an event occurs that impacts availability of the voice recognition, for
         * example audio permissions or policy values change.
         */
        void onVoiceAvailabilityImpacted();
    }

    public VoiceRecognitionHandler(
            OmniboxStub omniboxStub,
            LocationBarDataProvider locationBarDataProvider,
            AutocompleteCoordinator autocompleteCoordinator,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier) {
        this(
                omniboxStub,
                locationBarDataProvider,
                autocompleteCoordinator,
                windowAndroid,
                profileSupplier,
                new VoiceRecognitionIntentHandler(windowAndroid));
    }

    VoiceRecognitionHandler(
            OmniboxStub omniboxStub,
            LocationBarDataProvider locationBarDataProvider,
            AutocompleteCoordinator autocompleteCoordinator,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            VoiceRecognitionIntentHandler intentHandler) {
        mOmniboxStub = omniboxStub;
        mLocationBarDataProvider = locationBarDataProvider;
        mAutocompleteCoordinator = autocompleteCoordinator;
        mProfileSupplier = profileSupplier;
        mIntentHandler = intentHandler;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(
                mCallbackController.makeCancelable(profile -> notifyVoiceAvailabilityImpacted()));
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        mIntentHandler.destroy();
    }

    private void notifyVoiceAvailabilityImpacted() {
        for (Observer o : mObservers) {
            o.onVoiceAvailabilityImpacted();
        }
    }

    /**
     * Instantiated when a voice search is performed to monitor the web contents for a navigation to
     * be started so we can notify the render frame that a user gesture has been performed. This
     * allows autoplay of the voice response for search results.
     */
    private final class VoiceSearchWebContentsObserver extends WebContentsObserver {
        public VoiceSearchWebContentsObserver(WebContents webContents) {
            super(webContents);
        }

        /**
         * Forces the user gesture flag to be set on a render frame if the URL being navigated to is
         * a SRP.
         *
         * @param url The URL for the navigation that started, so we can ensure that what we're
         *     navigating to is actually a SRP.
         */
        private void setReceivedUserGesture(GURL url) {
            WebContents webContents = getWebContents();
            if (webContents == null) return;

            RenderFrameHost renderFrameHost = webContents.getMainFrame();
            if (renderFrameHost == null) return;

            Profile profile = mProfileSupplier.get();
            if (profile == null) return;
            if (TemplateUrlServiceFactory.getForProfile(profile)
                    .isSearchResultsPageFromDefaultSearchProvider(url)) {
                renderFrameHost.notifyUserActivation();
            }
        }

        @Override
        public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
            if (navigation.hasCommitted() && !navigation.isErrorPage()) {
                setReceivedUserGesture(navigation.getUrl());
            }
            observe(null);
        }
    }

    void handleTranscriptionResult(List<VoiceResult> voiceResults) {
        assert mAutocompleteCoordinator != null;

        List<VoiceResult> classifiedResults = new ArrayList<>();
        Profile profile = mProfileSupplier.get();
        if (profile != null) {
            for (VoiceResult result : voiceResults) {
                String matchText = result.getMatch();
                // Remove any spaces in the voice search match when determining whether it
                // appears to be a URL. This is to prevent cases like (
                // "tech crunch.com" and "www. engadget .com" from not appearing like URLs)
                // from not navigating to the URL.
                // If the string appears to be a URL, then use it instead of the string returned
                // from
                // the voice engine.
                String culledString = matchText.replaceAll(" ", "");
                AutocompleteMatch match = AutocompleteCoordinator.classify(profile, culledString);

                String urlOrSearchQuery;
                if (match == null || match.isSearchSuggestion()) {
                    urlOrSearchQuery = matchText;
                } else {
                    urlOrSearchQuery = culledString;
                }
                classifiedResults.add(new VoiceResult(urlOrSearchQuery, result.getConfidence()));
            }
        } else {
            classifiedResults.addAll(voiceResults);
        }

        mAutocompleteCoordinator.onVoiceResults(classifiedResults);

        // Since the intent handler already validated results, we can assume it is non-empty
        VoiceResult topResult = classifiedResults.get(0);
        String topResultQuery = topResult.getMatch();

        FuseboxSessionState sessionState = mLocationBarDataProvider.getFuseboxSessionState();
        @AutocompleteRequestType
        int requestType =
                sessionState != null
                        ? sessionState.getAutocompleteInput().getRequestType()
                        : AutocompleteRequestType.SEARCH;

        if (topResult.getConfidence() < VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD) {
            beginInputWithVerbatimText(topResultQuery, requestType);
            return;
        }

        // Since voice was used, we need to let the frame know that there was a user gesture.
        Tab currentTab =
                mLocationBarDataProvider != null ? mLocationBarDataProvider.getTab() : null;
        if (currentTab != null) {
            if (mVoiceSearchWebContentsObserver != null) {
                mVoiceSearchWebContentsObserver.observe(null);
                mVoiceSearchWebContentsObserver = null;
            }
            if (currentTab.getWebContents() != null) {
                mVoiceSearchWebContentsObserver =
                        new VoiceSearchWebContentsObserver(currentTab.getWebContents());
            }
        }

        mOmniboxStub.loadUrlFromVoice(topResultQuery);
    }

    private void beginInputWithVerbatimText(
            String query, @AutocompleteRequestType int requestType) {
        AutocompleteInput input =
                new AutocompleteInput(OmniboxFocusReason.SEARCH_QUERY)
                        .setUserText(query)
                        .setSelection(TextSelection.SELECT_ALL)
                        .setRequestType(requestType);
        mOmniboxStub.beginInput(input);
    }

    /**
     * Triggers a voice recognition intent to allow the user to specify a search query.
     *
     * @param source The source of the voice recognition initiation, such as NTP or omnibox.
     * @param onCanceled The callback function to run when the voice recognition is canceled.
     */
    public void startVoiceRecognition(@VoiceInteractionSource int source, Runnable onCanceled) {
        ThreadUtils.assertOnUiThread();
        mIntentHandler.startVoiceRecognition(
                source,
                new VoiceRecognitionIntentHandler.RecognitionCallback() {
                    @Override
                    public void onCompleted(List<VoiceResult> results) {
                        handleTranscriptionResult(results);
                    }

                    @Override
                    public void onCanceled() {
                        if (onCanceled != null) {
                            onCanceled.run();
                        }
                    }

                    @Override
                    public void onAvailabilityImpacted() {
                        notifyVoiceAvailabilityImpacted();
                        if (onCanceled != null) {
                            onCanceled.run();
                        }
                    }
                });
    }

    /** Returns whether voice search is enabled on the current tab. */
    public boolean isVoiceSearchEnabled() {
        if (mLocationBarDataProvider == null) return false;
        if (mLocationBarDataProvider.isIncognito()) return false;

        return mIntentHandler.isVoiceSearchEnabled();
    }
}
