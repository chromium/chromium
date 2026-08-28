// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;

/**
 * Data class defining UI overrides for the {@link LocationBar}.
 *
 * <p><b>IMPORTANT</b>: This class is a <i>last resort ("nothing else worked")</i> mechanism. It
 * should <b>ONLY</b> be used if requirements absolutely cannot be accommodated through other
 * existing signals, models, or abstractions (such as {@link
 * org.chromium.components.omnibox.AutocompleteMatch.PageClassification}, {@link
 * LocationBarDataProvider}, or MVC property models).
 *
 * <p>Always check whether a relevant signal already exists (e.g. page classification) before
 * introducing or using an override.
 */
@NullMarked
public class LocationBarEmbedderUiOverrides {
    private final SettableNullableObservableSupplier<SideUiStateProvider>
            mSideUiStateProviderSupplier = ObservableSuppliers.createNullable();
    private boolean mForcedPhoneStyleOmnibox;
    private boolean mLensEntrypointAllowed;
    private boolean mVoiceEntrypointAllowed;
    private boolean mIsEmbedderControlledHint;
    private boolean mIsMainBrowserOmnibox;

    public LocationBarEmbedderUiOverrides() {
        mLensEntrypointAllowed = true;
        mVoiceEntrypointAllowed = true;
        mIsEmbedderControlledHint = false;
    }

    /**
     * Whether the omnibox embedder represents the main browser URL bar. This refers to whether the
     * omnibox instance was created through the toolbar or through search activity.
     */
    public boolean isMainBrowserOmnibox() {
        return mIsMainBrowserOmnibox;
    }

    /**
     * Specify that this omnibox embedder represents the main browser URL bar. This should only be
     * set when instantiated via the {@link ToolbarManager} and not in other cases such as {@link
     * SearchUiCoordinator}.
     *
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setIsMainBrowserOmnibox() {
        mIsMainBrowserOmnibox = true;
        return this;
    }

    /**
     * Whether a "phone-style" (full bleed, unrounded corners) omnibox suggestions list should be
     * used even when the screen width is >600dp.
     */
    public boolean isForcedPhoneStyleOmnibox() {
        return mForcedPhoneStyleOmnibox;
    }

    /**
     * Force a "phone-style" (full bleed, unrounded corners) omnibox suggestions list to be used
     * even when the screen width is >600dp.
     *
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setForcedPhoneStyleOmnibox() {
        mForcedPhoneStyleOmnibox = true;
        return this;
    }

    /** Whether Lens entrypoint should be offered to the user. */
    public boolean isLensEntrypointAllowed() {
        return mLensEntrypointAllowed;
    }

    /**
     * Specify whether Lens entrypoint should be offered to the user.
     *
     * @param isAllowed whether Lens entrypoint should be shown in the Location bar
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setLensEntrypointAllowed(boolean isAllowed) {
        mLensEntrypointAllowed = isAllowed;
        return this;
    }

    /** Whether Voice entrypoint should be offered to the user. */
    public boolean isVoiceEntrypointAllowed() {
        return mVoiceEntrypointAllowed;
    }

    /**
     * Specify whether Voice entrypoint should be offered to the user.
     *
     * @param isAllowed whether Voice entrypoint should be shown in the Location bar
     * @return {@code this} for call chaining
     */
    public LocationBarEmbedderUiOverrides setVoiceEntrypointAllowed(boolean isAllowed) {
        mVoiceEntrypointAllowed = isAllowed;
        return this;
    }

    /** Whether the hint text is explicitly controlled by the embedder. */
    public boolean isEmbedderControlledHint() {
        return mIsEmbedderControlledHint;
    }

    /**
     * Specify whether the hint text is completely controlled by the embedder. Set this to true for
     * contexts with specific hint text.
     *
     * @param isControlled whether embedder controls the hint.
     * @return {@code this} for call chaining.
     */
    public LocationBarEmbedderUiOverrides setEmbedderControlledHint(boolean isControlled) {
        mIsEmbedderControlledHint = isControlled;
        return this;
    }

    /** Returns the {@link SideUiStateProvider}. */
    public @Nullable SideUiStateProvider getSideUiStateProvider() {
        return mSideUiStateProviderSupplier.get();
    }

    /**
     * Specify the {@link SideUiStateProvider}.
     *
     * @param sideUiStateProvider The {@link SideUiStateProvider} object.
     */
    public void setSideUiStateProvider(SideUiStateProvider sideUiStateProvider) {
        mSideUiStateProviderSupplier.set(sideUiStateProvider);
    }

    /** Returns the {@link NullableObservableSupplier} for the {@link SideUiStateProvider}. */
    public NullableObservableSupplier<SideUiStateProvider> getSideUiStateProviderSupplier() {
        return mSideUiStateProviderSupplier;
    }
}
