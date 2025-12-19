// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.omnibox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_QUERY_TILES;

import org.mockito.Mock;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Helper to simulate Omnibox suggestions being received from the server.
 *
 * <p>Sets up a fake {@link AutocompleteController} which accepts calls from the test to simulate
 * suggestions being received and notifies a OnSuggestionsReceivedListener.
 */
public class FakeOmniboxSuggestions {
    private static FakeOmniboxSuggestions sInstance;

    @Mock AutocompleteControllerJni mControllerJni;

    private final Map<Profile, AutocompleteController> mControllers = new HashMap<>();
    private final Map<Profile, Set<AutocompleteController.OnSuggestionsReceivedListener>>
            mListenersMap = new HashMap<>();

    public FakeOmniboxSuggestions() {
        if (sInstance != null) {
            throw new IllegalStateException("Do not create more than one FakeOmniboxSuggestions");
        }
        sInstance = this;
    }

    public void destroy() {
        sInstance = null;
    }

    /**
     * Simulate an autocomplete suggestion.
     *
     * @param profile the profile to simulate suggestions for
     * @param startOfTerm start of term that user has input
     * @param autocompletion the rest of the term suggested as autocompletion
     */
    public void simulateAutocompleteSuggestion(
            Profile profile, String startOfTerm, String autocompletion) {
        AutocompleteMatch match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText(startOfTerm + autocompletion)
                        .setInlineAutocompletion(autocompletion)
                        .setAllowedToBeDefaultMatch(true)
                        .build();
        AutocompleteResult result =
                AutocompleteResult.fromCache(
                        List.of(match),
                        GroupsInfo.newBuilder().putGroupConfigs(1, SECTION_QUERY_TILES).build());

        // Simulate suggestions received.
        Set<AutocompleteController.OnSuggestionsReceivedListener> listeners =
                mListenersMap.get(profile);
        if (listeners != null) {
            for (AutocompleteController.OnSuggestionsReceivedListener listener : listeners) {
                ThreadUtils.postOnUiThread(
                        () -> listener.onSuggestionsReceived(result, /* isFinal= */ true));
            }
        }
    }

    public void initMocks() {
        mControllerJni = mock(AutocompleteControllerJni.class);
        AutocompleteControllerJni.setInstanceForTesting(mControllerJni);

        when(mControllerJni.getForProfile(any()))
                .thenAnswer(
                        inv -> {
                            Profile profile = inv.getArgument(0);
                            return getOrCreateController(profile);
                        });
    }

    private AutocompleteController getOrCreateController(Profile profile) {
        if (!mControllers.containsKey(profile)) {
            AutocompleteController controller = mock(AutocompleteController.class);
            mControllers.put(profile, controller);
            mListenersMap.put(profile, new HashSet<>());

            doAnswer(
                            inv -> {
                                mListenersMap.get(profile).add(inv.getArgument(0));
                                return null;
                            })
                    .when(controller)
                    .addOnSuggestionsReceivedListener(any());
            doAnswer(
                            inv -> {
                                mListenersMap.get(profile).remove(inv.getArgument(0));
                                return null;
                            })
                    .when(controller)
                    .removeOnSuggestionsReceivedListener(any());
        }
        return mControllers.get(profile);
    }
}
