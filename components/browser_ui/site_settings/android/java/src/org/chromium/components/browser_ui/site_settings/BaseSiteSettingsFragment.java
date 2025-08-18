// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.FragmentSettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsItemBackgroundDecoration;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsStylingController;

import java.util.Objects;

/** Preference fragment for showing the Site Settings UI. */
@NullMarked
public abstract class BaseSiteSettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsNavigation {
    private @Nullable SiteSettingsDelegate mSiteSettingsDelegate;
    private @Nullable SettingsNavigation mSettingsNavigation;

    /**
     * The item decoration that applies the background to the settings items. Null if the settings
     * containment feature is not enabled.
     */
    private @Nullable SettingsItemBackgroundDecoration mItemBackgroundDecoration;

    /**
     * Sets the SiteSettingsDelegate instance this Fragment should use.
     *
     * <p>This should be called by the embedding Activity.
     */
    public void setSiteSettingsDelegate(SiteSettingsDelegate client) {
        assert mSiteSettingsDelegate == null;
        mSiteSettingsDelegate = client;
    }

    /** @return the SiteSettingsDelegate instance to use when rendering the Site Settings UI. */
    public SiteSettingsDelegate getSiteSettingsDelegate() {
        assert mSiteSettingsDelegate != null : "SiteSettingsDelegate not set";
        return mSiteSettingsDelegate;
    }

    /** @return Whether a SiteSettingsDelegate instance has been assigned to this Fragment. */
    public boolean hasSiteSettingsDelegate() {
        return mSiteSettingsDelegate != null;
    }

    @NonNull
    @Override
    public RecyclerView onCreateRecyclerView(
            @NonNull LayoutInflater inflater,
            @NonNull ViewGroup parent,
            @Nullable Bundle savedInstanceState) {
        RecyclerView recyclerView =
                super.onCreateRecyclerView(inflater, parent, savedInstanceState);

        if (getSiteSettingsDelegate().isSettingsContainmentEnabled()) {
            mItemBackgroundDecoration = new SettingsItemBackgroundDecoration(getContext());
            recyclerView.addItemDecoration(mItemBackgroundDecoration);
        }
        return recyclerView;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        if (getSiteSettingsDelegate().isSettingsContainmentEnabled()) {
            updateBackgrounds(getListView());
        }
    }

    /** Updates the background of all the visible preferences on the settings screen. */
    protected void updateBackgrounds(RecyclerView recyclerView) {
        recyclerView.post(
                () -> {
                    if (mItemBackgroundDecoration == null) return;
                    SettingsStylingController stylingController =
                            new SettingsStylingController(
                                    Objects.requireNonNull(getContext()), getPreferenceScreen());

                    mItemBackgroundDecoration.updateBackgroundStyleDetails(
                            stylingController.generateBackgroundStyleDetails());
                    recyclerView.invalidateItemDecorations();
                });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mSiteSettingsDelegate != null) {
            mSiteSettingsDelegate.onDestroyView();
        }
    }

    @Override
    public void setSettingsNavigation(SettingsNavigation settingsNavigation) {
        mSettingsNavigation = settingsNavigation;
    }

    /** Returns the associated {@link SettingsNavigation}. */
    public @Nullable SettingsNavigation getSettingsNavigation() {
        return mSettingsNavigation;
    }
}
