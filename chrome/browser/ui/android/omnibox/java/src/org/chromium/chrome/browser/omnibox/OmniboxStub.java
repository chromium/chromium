// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.components.omnibox.AutocompleteInput;

/**
 * Handles user interaction with the stubbed Omnibox (a.k.a. fakebox) used in the pages such as NTP
 * and tasks surface.
 */
@NullMarked
public interface OmniboxStub {
    /**
     * Set the omnibox to have focus or not.
     *
     * <p>Updates passed AutocompleteInput instance so it correctly reflects the current page URL,
     * title, classification, and focus time, bringing the Fusebox to focus with the supplied data.
     * When null instance is passed the focus is cleared.
     *
     * @param input The AutocompleteInput object with all the details for the focus operation. If
     *     null, the focus will be cleared.
     */
    void setUrlBarFocus(@Nullable AutocompleteInput input);

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
}
