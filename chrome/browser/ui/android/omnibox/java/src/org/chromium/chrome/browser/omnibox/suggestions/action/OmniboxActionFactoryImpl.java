// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import androidx.annotation.NonNull;

import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionFactory;
import org.chromium.components.omnibox.action.OmniboxActionFactoryJni;
import org.chromium.components.omnibox.action.OmniboxPedalId;

/**
 * A factory creating the OmniboxAction instances.
 */
public class OmniboxActionFactoryImpl implements OmniboxActionFactory {
    private static OmniboxActionFactoryImpl sFactory;

    /** Private constructor to suppress direct instantiation of this class. */
    private OmniboxActionFactoryImpl() {}

    /**
     * Creates (if not already created) and returns the App-wide instance of the
     * OmniboxActionFactory.
     */
    public static @NonNull OmniboxActionFactoryImpl get() {
        if (sFactory == null) {
            sFactory = new OmniboxActionFactoryImpl();
        }
        return sFactory;
    }

    /**
     * Initialize (if not already done) and return the instance of the OmniboxActionFactory
     * to be used until application is destroyed.
     */
    public void initNativeFactory() {
        OmniboxActionFactoryJni.get().setFactory(this);
    }

    /**
     * Destroy the OmniboxActionFactory if previously created.
     */
    public void destroyNativeFactory() {
        OmniboxActionFactoryJni.get().setFactory(null);
    }

    @Override
    public @NonNull OmniboxAction buildOmniboxPedal(
            @NonNull String hint, @OmniboxPedalId int pedalId) {
        return new OmniboxPedal(hint, pedalId);
    }

    @Override
    public @NonNull OmniboxAction buildActionInSuggest(@NonNull String hint,
            /* EntityInfoProto.ActionInfo.ActionType */ int actionType, @NonNull String actionUri) {
        return new OmniboxActionInSuggest(hint, actionType, actionUri);
    }

    @Override
    public @NonNull OmniboxAction buildHistoryClustersAction(
            @NonNull String hint, @NonNull String query) {
        return new HistoryClustersAction(hint, query);
    }
}
