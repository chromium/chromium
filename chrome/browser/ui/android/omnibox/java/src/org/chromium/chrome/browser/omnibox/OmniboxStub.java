// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.components.omnibox.AutocompleteRequestType;

/**
 * Handles user interaction with the stubbed Omnibox (a.k.a. fakebox) used in the pages such as NTP
 * and tasks surface.
 */
@NullMarked
public interface OmniboxStub {
    /**
     * Signal a {@link UrlBar} focus change request.
     *
     * @param shouldBeFocused Whether the focus should be requested or cleared. True requests focus
     *     and False clears focus.
     * @param pastedText The given pasted text when focus, which could be null.
     * @param selectText Whether the pasted text should be selected.
     * @param reason The given reason.
     * @param requestType The request type (tool/mode) to focus the Omnibox for.
     */
    void setUrlBarFocus(
            boolean shouldBeFocused,
            @Nullable String pastedText,
            boolean selectText,
            @OmniboxFocusReason int reason,
            @AutocompleteRequestType int requestType);

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
