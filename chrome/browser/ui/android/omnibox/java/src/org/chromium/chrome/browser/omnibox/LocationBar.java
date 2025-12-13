// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;

/** Container that holds the {@link UrlBar} and SSL state related with the current {@link Tab}. */
@NullMarked
public interface LocationBar {
    /** Handle all necessary tasks that can be delayed until initialization completes. */
    default void onDeferredStartup() {}

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    void updateVisualsForState();

    /**
     * Sets whether the location bar should have a layout showing a title.
     *
     * @param showTitle Whether the title should be shown.
     */
    void setShowTitle(boolean showTitle);

    /**
     * Sends an accessibility event to the URL bar to request accessibility focus on it (e.g. for
     * TalkBack).
     */
    default void requestUrlBarAccessibilityFocus() {}

    /**
     * Triggers the cursor to be visible in the UrlBar without triggering any of the focus animation
     * logic.
     *
     * <p>Only applies to devices with a hardware keyboard attached.
     */
    void showUrlBarCursorWithoutFocusAnimations();

    /**
     * Notifies the LocationBar to take necessary action after exiting from the NTP, while a
     * hardware keyboard is connected. If the URL bar was previously focused on the NTP due to a
     * connected keyboard, a navigation away from the NTP should clear this focus before filling the
     * current tab's URL.
     */
    void clearUrlBarCursorWithoutFocusAnimations();

    /** Selects all of the editable text in the {@link UrlBar}. */
    void selectAll();

    /**
     * Reverts any pending edits of the location bar and reset to the page state. This does not
     * change the focus state of the location bar.
     */
    void revertChanges();

    /** Returns {@link ViewGroup} that this container holds. */
    View getContainerView();

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     * bubble anchor view... which should be moved out of tab and perhaps into the status bar icon
     * component.
     *
     * @return The view containing the security icon.
     */
    View getSecurityIconView();

    /** Returns the {@link VoiceRecognitionHandler} associated with this LocationBar. */
    default @Nullable VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return null;
    }

    /**
     * Returns a (@link OmniboxStub}.
     *
     * <p>TODO(crbug.com/40153747): Inject OmniboxStub where needed and remove this method.
     */
    @Nullable OmniboxStub getOmniboxStub();

    /** Returns the UrlBarData currently in use by the URL bar inside this location bar. */
    UrlBarData getUrlBarData();

    /** Adds an observer for suggestions scroll events. */
    default void addOmniboxSuggestionsDropdownScrollListener(
            OmniboxSuggestionsDropdownScrollListener listener) {}

    /** Removes an observer for suggestions scroll events. */
    default void removeOmniboxSuggestionsDropdownScrollListener(
            OmniboxSuggestionsDropdownScrollListener listener) {}

    @Nullable OmniboxSuggestionsVisualState getOmniboxSuggestionsVisualState();

    /**
     * Toggle showing only the origin portion of the URL (as opposed to the default behavior of
     * showing the max amount of the url, prioritizing the origin)
     */
    default void setShowOriginOnly(boolean showOriginOnly) {}

    /** Toggle the url bar's text size to be small or normal sized. */
    default void setUrlBarUsesSmallText(boolean useSmallText) {}

    /**
     * Toggle whether the status icon should be shown/hidden for secure origins in steady state.
     *
     * <p>This method should be used to control whether the Status Icon should be shown in the
     * steady Omnibox state, allowing the alternative presentations (such as the MiniOriginBar) to
     * reduce the clutter.
     */
    default void setShowStatusIconForSecureOrigins(boolean showStatusIconForSecureOrigins) {}

    /** Gets the height of the url bar view contained by the location bar. */
    default float getUrlBarHeight() {
        return 0;
    }

    /**
     * Called whenever the NTP could have been entered or exited (e.g. tab content changed, tab
     * navigated to from the tab strip/tab switcher, etc.). If the user is on a tablet and indeed
     * entered or exited from the NTP, we will check the following cases: 1. If a11y is enabled, we
     * will request a11y focus on the omnibox (e.g. for TalkBack) on the NTP. 2. If a keyboard is
     * plugged in, we will show the URL bar cursor (without focus animations) on entering the NTP.
     * 3. If a keyboard is plugged in, we will clear focus established in #2 above on exiting from
     * the NTP.
     */
    default void maybeShowOrClearCursorInLocationBar() {}

    /**
     * Called when the zoom level of the page has changed. Note: The zoom level value is not
     * represented as a percentage (e.g., 100.0) or a fraction (e.g., 1.0). Instead, it uses an
     * internal table where a value of `0.0` corresponds to 100% zoom. The default zoom level can
     * differ if the user has set a preference. For the complete mapping of values to zoom
     * percentages, see the zoom table variable. Read more at {@link PageZoomUtils}
     *
     * @param zoomLevel The new zoom level.
     */
    default void onZoomLevelChanged(double zoomLevel) {}

    /** Destroys the LocationBar. */
    void destroy();
}
