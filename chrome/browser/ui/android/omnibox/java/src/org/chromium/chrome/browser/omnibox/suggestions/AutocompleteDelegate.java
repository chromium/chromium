// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.LoadUrlParams;

/** Provides the additional functionality to trigger and interact with autocomplete suggestions. */
public interface AutocompleteDelegate extends UrlBarDelegate {

    /** Called when loadUrl is done on a {@link Tab}. */
    interface AutocompleteLoadCallback {
        void onLoadUrl(LoadUrlParams params, LoadUrlResult loadUrlResult);
    }

    /** Notified that the URL text has changed. */
    void onUrlTextChanged();

    /**
     * Notified that suggestions have changed.
     *
     * @param defaultMatch default AutocompleteMatch.
     */
    void onSuggestionsChanged(@Nullable AutocompleteMatch defaultMatch);

    /**
     * Requests the keyboard visibility update.
     *
     * @param shouldShow When true, keyboard should be made visible.
     * @param delayHide when true, hiding will commence after brief delay.
     */
    void setKeyboardVisibility(boolean shouldShow, boolean delayHide);

    /**
     * @return Reports whether keyboard (whether software or hardware) is active. Software keyboard
     *     is reported as active whenever it is visible on screen; hardware keyboard is reported as
     *     active when it is connected.
     */
    boolean isKeyboardActive();

    /**
     * Requests that the given URL be loaded.
     *
     * @param omniboxLoadUrlParams parameters describing the url load.
     */
    void loadUrl(OmniboxLoadUrlParams omniboxLoadUrlParams);

    /**
     * @return Whether the omnibox was focused via the NTP fakebox.
     */
    boolean didFocusUrlFromFakebox();

    /**
     * @return Whether the URL currently has focus.
     */
    boolean isUrlBarFocused();

    /* Requests to show default browser promo when user pasting an URL. */
    void maybeShowDefaultBrowserPromo();
}
