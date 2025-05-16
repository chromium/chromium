// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.suggestions.mostvisited;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * A fake implementation of MostVisitedSites that returns a fixed list of most visited sites.
 *
 * <p>Once the observer is set (through {@link #setObserver(Observer, int)}), updates to the data
 * must be made on the UI thread, as they can result in UI manipulations.
 */
@NullMarked
public class FakeMostVisitedSites implements MostVisitedSites {
    private final List<GURL> mBlocklistedUrls = new ArrayList<>();

    private List<SiteSuggestion> mSites = new ArrayList<>();
    private @Nullable Observer mObserver;

    // CustomLinkOperations -> MostVisitedSites implementation.
    @Override
    public boolean addCustomLink(String name, @Nullable GURL url, @Nullable Integer pos) {
        // TODO (crbug.com/397421764): Implement when needed by tests.
        return false;
    }

    @Override
    public boolean assignCustomLink(GURL keyUrl, String name, @Nullable GURL url) {
        // TODO (crbug.com/397421764): Implement when needed by tests.
        return false;
    }

    @Override
    public boolean deleteCustomLink(GURL keyUrl) {
        // TODO (crbug.com/397421764): Implement when needed by tests.
        return false;
    }

    @Override
    public boolean hasCustomLink(GURL keyUrl) {
        // TODO (crbug.com/397421764): Implement when needed by tests.
        return false;
    }

    @Override
    public boolean reorderCustomLink(GURL keyUrl, int newPos) {
        // TODO (crbug.com/397421764): Implement when needed by tests.
        return false;
    }

    // MostVisitedSites implementation.
    @Override
    public void destroy() {}

    @Override
    public void setObserver(Observer observer, int numResults) {
        mObserver = observer;
        notifyTileSuggestionsAvailable();
    }

    @Override
    public void addBlocklistedUrl(GURL url) {
        mBlocklistedUrls.add(url);
    }

    @Override
    public void removeBlocklistedUrl(GURL url) {
        mBlocklistedUrls.remove(url);
    }

    @Override
    public void recordPageImpression(int tilesCount) {
        // Metrics are stubbed out.
    }

    @Override
    public void recordTileImpression(Tile tile) {
        // Metrics are stubbed out.
    }

    @Override
    public void recordOpenedMostVisitedItem(Tile tile) {
        //  Metrics are stubbed out.
    }

    /** @return Whether {@link #addBlocklistedUrl} has been called on the given URL. */
    public boolean isUrlBlocklisted(GURL url) {
        return mBlocklistedUrls.contains(url);
    }

    /**
     * Sets new tile suggestion data.
     *
     * <p>If there is an observer it will be notified and the call has to be made on the UI thread.
     */
    public void setTileSuggestions(List<SiteSuggestion> suggestions) {
        mSites = new ArrayList<>(suggestions);
        notifyTileSuggestionsAvailable();
    }

    /**
     * Sets new tile suggestion data.
     *
     * If there is an observer it will be notified and the call has to be made on the UI thread.
     */
    public void setTileSuggestions(SiteSuggestion... suggestions) {
        setTileSuggestions(Arrays.asList(suggestions));
    }

    /**
     * Sets new tile suggestion data, generating fake data for the missing properties.
     *
     * <p>If there is an observer it will be notified and the call has to be made on the UI thread.
     *
     * @param urls The URLs of the site suggestions.
     * @see #setTileSuggestions(SiteSuggestion[])
     */
    public void setTileSuggestions(String... urls) {
        setTileSuggestions(createSiteSuggestions(urls));
    }

    /** @return An unmodifiable view of the current list of sites. */
    public List<SiteSuggestion> getCurrentSites() {
        return Collections.unmodifiableList(mSites);
    }

    public static List<SiteSuggestion> createSiteSuggestions(String... urls) {
        List<SiteSuggestion> suggestions = new ArrayList<>(urls.length);
        for (String url : urls) suggestions.add(createSiteSuggestion(url));
        return suggestions;
    }

    public static SiteSuggestion createSiteSuggestion(String url) {
        return createSiteSuggestion(url, url);
    }

    public static SiteSuggestion createSiteSuggestion(String title, String url) {
        return new SiteSuggestion(
                title,
                new GURL(url),
                TileTitleSource.TITLE_TAG,
                TileSource.TOP_SITES,
                TileSectionType.PERSONALIZED);
    }

    private void notifyTileSuggestionsAvailable() {
        if (mObserver == null) return;

        // Notifying the observer usually results in view modifications, so this call should always
        // happen on the UI thread. We assert we do it here to make detecting related mistakes in
        // tests more easily.
        // To make initialisation easier, we only enforce that once the observer is set, using it as
        // a signal that the test started and this is not the setup anymore.
        ThreadUtils.assertOnUiThread();

        mObserver.onSiteSuggestionsAvailable(mSites);
    }
}
