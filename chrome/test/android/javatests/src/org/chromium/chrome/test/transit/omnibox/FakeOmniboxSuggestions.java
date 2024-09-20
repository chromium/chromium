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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;

import java.util.List;

/**
 * Helper to simulate Omnibox suggestions being received from the server.
 *
 * <p>Sets up a fake {@link AutocompleteController} which accepts calls from the test to simulate
 * suggestions being received and notifies a OnSuggestionsReceivedListener.
 */
public class FakeOmniboxSuggestions {
    private static FakeOmniboxSuggestions sInstance;

    @Mock AutocompleteController mController;
    @Mock AutocompleteControllerJni mControllerJni;

    private AutocompleteController.OnSuggestionsReceivedListener mListener;

    public FakeOmniboxSuggestions(JniMocker jniMocker) {
        if (sInstance != null) {
            throw new IllegalStateException("Do not create more than one FakeOmniboxSuggestions");
        }
        sInstance = this;
        mController = mock(AutocompleteController.class);
        mControllerJni = mock(AutocompleteControllerJni.class);
        jniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mControllerJni);

        when(mControllerJni.getForProfile(any())).thenReturn(mController);
        doAnswer(
                        inv -> {
                            // Currently supports only one listener, assert if this changes.
                            assert mListener == null || mListener == inv.getArgument(0);
                            mListener = inv.getArgument(0);
                            return null;
                        })
                .when(mController)
                .addOnSuggestionsReceivedListener(any());
    }

    /**
     * Simulate an autocomplete suggestion.
     *
     * @param startOfTerm start of term that user has input
     * @param autocompletion the rest of the term suggested as autocompletion
     */
    public void simulateAutocompleteSuggestion(String startOfTerm, String autocompletion) {
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
        simulateSuggestionsReceived(result);
    }

    private void simulateSuggestionsReceived(AutocompleteResult result) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mListener.onSuggestionsReceived(result, /* isFinal= */ true));
    }
}
