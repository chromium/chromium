// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;

import java.util.function.Supplier;

/**
 * Immutable context object containing common UI dependencies for Autocomplete suggestion
 * processors.
 *
 * <p>This class centralizes frequently-used parameters to reduce method signature complexity and
 * improve maintainability when creating suggestion processors. All fields are public and final to
 * provide direct access while ensuring immutability.
 */
@NullMarked
public final class AutocompleteUIContext {
    /** Android context for UI operations and resource access. */
    public final Context context;

    /** Host responding to suggestion events. */
    public final SuggestionHost host;

    /** Provider for querying and editing the Omnibox text state. */
    public final UrlBarEditingTextStateProvider textProvider;

    /** Image supplier for suggestion icons, empty on low-memory devices. */
    public final @Nullable OmniboxImageSupplier imageSupplier;

    /** Bookmark state provider for determining bookmark status of suggestions. */
    public final BookmarkState bookmarkState;

    /** Activity tab supplier, may provide null tab when no active tab exists. */
    public final Supplier<@Nullable Tab> activityTabSupplier;

    /** Share delegate supplier, may be null if sharing functionality is not available. */
    public final @Nullable Supplier<ShareDelegate> shareDelegateSupplier;

    /** Toolbar position supplier, reporting the on-screen position of the Toolbar. */
    public final ObservableSupplier<@ControlsPosition Integer> toolbarPositionSupplier;

    /**
     * @param context Android context for UI operations
     * @param host Component for creating suggestion view delegates
     * @param textProvider Provider for Omnibox text state
     * @param imageSupplier Image supplier for suggestion icons
     * @param bookmarkState Bookmark state provider
     * @param activityTabSupplier Activity tab supplier
     * @param shareDelegateSupplier Share delegate supplier, may be null
     */
    public AutocompleteUIContext(
            Context context,
            SuggestionHost host,
            UrlBarEditingTextStateProvider textProvider,
            @Nullable OmniboxImageSupplier imageSupplier,
            BookmarkState bookmarkState,
            Supplier<@Nullable Tab> activityTabSupplier,
            @Nullable Supplier<ShareDelegate> shareDelegateSupplier,
            ObservableSupplier<@ControlsPosition Integer> toolbarPositionSupplier) {
        this.context = context;
        this.host = host;
        this.textProvider = textProvider;
        this.imageSupplier = imageSupplier;
        this.bookmarkState = bookmarkState;
        this.activityTabSupplier = activityTabSupplier;
        this.shareDelegateSupplier = shareDelegateSupplier;
        this.toolbarPositionSupplier = toolbarPositionSupplier;
    }
}
