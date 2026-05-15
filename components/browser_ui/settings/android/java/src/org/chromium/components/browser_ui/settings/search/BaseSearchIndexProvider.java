// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import android.content.Context;

import androidx.annotation.XmlRes;

import org.chromium.build.annotations.NullMarked;

import java.util.Map;
import java.util.Set;

/**
 * A basic SearchIndexProvider implementation that retrieves the preferences to index from xml
 * resource only.
 */
@NullMarked
public class BaseSearchIndexProvider implements SearchIndexProvider {

    public static int INDEX_OPT_OUT = -1;

    private final int mXmlRes;
    private final String mPrefFragment;
    private final boolean mIsSearchable;

    /**
     * Constructor for Fragment without XML resource.
     *
     * @param prefFragment {@link PreferenceFragment} owning this {@link SearchIndexProvider}.
     */
    public BaseSearchIndexProvider(String prefFragment) {
        this(prefFragment, 0, /* isSearchable= */ true);
    }

    /**
     * Constructor for Fragment.
     *
     * @param prefFragment {@link PreferenceFragment} owning this {@link SearchIndexProvider}.
     * @param xmlRes Preference XML resource.
     */
    public BaseSearchIndexProvider(String prefFragment, @XmlRes int xmlRes) {
        this(prefFragment, xmlRes, /* isSearchable= */ true);
    }

    /**
     * Constructor for Fragment.
     *
     * @param prefFragment {@link PreferenceFragment} owning this {@link SearchIndexProvider}.
     * @param xmlRes Preference XML resource.
     * @param isSearchable If true, preferences are indexed and appear in user search results. If
     *     false, the XML is parsed solely to build the parent-child structural graph (e.g., for
     *     generating deep link breadcrumbs) but the items will remain hidden from user search
     *     queries.
     */
    public BaseSearchIndexProvider(String prefFragment, @XmlRes int xmlRes, boolean isSearchable) {
        mPrefFragment = prefFragment;
        mXmlRes = xmlRes;
        mIsSearchable = isSearchable && xmlRes != INDEX_OPT_OUT;
    }

    /**
     * Returns the unique id for a child pref.
     *
     * @param childPrefName The name of the child pref.
     * @return The unique id for that child pref.
     */
    public String getUniqueId(String childPrefName) {
        return PreferenceParser.createUniqueId(mPrefFragment, childPrefName);
    }

    /** Returns the name of the associated {@link PreferenceFragment}. */
    @Override
    public String getPrefFragmentName() {
        return mPrefFragment;
    }

    /** Returns the Preference XML resource. */
    @Override
    public @XmlRes int getXmlRes() {
        return mXmlRes;
    }

    /**
     * Returns whether the preferences from this provider should appear in user search results.
     *
     * <p>If this returns {@code false}, the preferences are still parsed to build the structural
     * parent-child graph (used for generating breadcrumbs on deep links), but they are explicitly
     * hidden from the search results UI.
     *
     * @return {@code true} if searchable, {@code false} if used for structure only.
     */
    @Override
    public boolean isSearchable() {
        return mIsSearchable;
    }

    @Override
    public void registerFragmentHeaders(
            Context context,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap,
            Set<String> processedFragments) {
        PreferenceParser.parseAndRegisterHeaders(
                context, mXmlRes, mPrefFragment, indexData, providerMap, processedFragments);
    }

    @Override
    public void initPreferenceXml(
            Context context,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap) {
        if (mXmlRes != 0 && mXmlRes != INDEX_OPT_OUT) {
            PreferenceParser.parseAndPopulate(
                    context,
                    mXmlRes,
                    indexData,
                    mPrefFragment,
                    getExtras(context),
                    providerMap,
                    isSearchable());
        }
    }
}
