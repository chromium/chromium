// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.url.GURL;

/**
 * Handles user interaction with the stubbed Omnibox (a.k.a. fakebox) used in the pages such as NTP
 * and tasks surface.
 */
@NullMarked
public interface OmniboxStub {
    /**
     * Begins an Omnibox input session with the given input. This will typically focus the Omnibox
     * and initialize autocomplete.
     *
     * @param input The AutocompleteInput object with details for the focus operation.
     */
    void beginInput(AutocompleteInput input);

    /**
     * Ends the current Omnibox input session. This will typically clear the focus from the Omnibox.
     */
    void endInput();

    /**
     * @return Whether the URL bar is currently focused.
     */
    boolean isUrlBarFocused();

    /**
     * Get the {@link VoiceRecognitionHandler}.
     *
     * @return the {@link VoiceRecognitionHandler}
     */
    @Nullable VoiceRecognitionHandler getVoiceRecognitionHandler();

    /**
     * Adds a URL focus change listener that will be notified when the URL gains or loses focus.
     *
     * @param listener The listener to be registered.
     */
    default void addUrlFocusChangeListener(UrlFocusChangeListener listener) {}

    /**
     * Removes a URL focus change listener that was previously added.
     *
     * @param listener The listener to be removed.
     */
    default void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {}

    /** Returns whether the Lens is currently enabled. */
    boolean isLensEnabled(@LensEntryPoint int lensEntryPoint);

    /**
     * Launches Lens from an entry point.
     *
     * @param lensEntryPoint the Lens entry point.
     */
    void startLens(@LensEntryPoint int lensEntryPoint);

    // Methods migrated from VoiceRecognitionHandler.Delegate

    /**
     * Loads the provided URL, assumes the PageTransition type is TYPED.
     *
     * @param url The URL to load.
     */
    // TODO(crbug.com/477922724): migrate to loadUrl()
    void loadUrlFromVoice(GURL url);
}
