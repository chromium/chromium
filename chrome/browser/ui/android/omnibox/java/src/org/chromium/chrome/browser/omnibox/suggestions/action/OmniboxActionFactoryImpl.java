// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionFactory;
import org.chromium.components.omnibox.action.OmniboxActionFactoryJni;
import org.chromium.components.omnibox.action.OmniboxPedalId;

/** A factory creating the OmniboxAction instances. */
@NullMarked
public class OmniboxActionFactoryImpl implements OmniboxActionFactory {
    private static @Nullable OmniboxActionFactoryImpl sFactory;
    private boolean mDialerAvailable;

    /** Private constructor to suppress direct instantiation of this class. */
    private OmniboxActionFactoryImpl() {}

    /** Initialize the factory. Called before native code is ready. */
    public OmniboxActionFactoryImpl setDialerAvailable(boolean dialerAvailable) {
        mDialerAvailable = dialerAvailable;
        return this;
    }

    /**
     * Creates (if not already created) and returns the App-wide instance of the
     * OmniboxActionFactory.
     */
    public static OmniboxActionFactoryImpl get() {
        if (sFactory == null) {
            sFactory = new OmniboxActionFactoryImpl();
        }
        return sFactory;
    }

    /**
     * Initialize (if not already done) and return the instance of the OmniboxActionFactory to be
     * used until application is destroyed.
     */
    public void initNativeFactory() {
        OmniboxActionFactoryJni.get().setFactory(this);
    }

    /** Destroy the OmniboxActionFactory if previously created. */
    public void destroyNativeFactory() {
        OmniboxActionFactoryJni.get().setFactory(null);
    }

    @Override
    public @Nullable OmniboxAction buildOmniboxPedal(
            long nativeInstance,
            String hint,
            String accessibilityHint,
            @OmniboxPedalId int pedalId) {
        return new OmniboxPedal(nativeInstance, hint, accessibilityHint, pedalId);
    }

    @Override
    public @Nullable OmniboxAction buildActionInSuggest(
            long nativeInstance,
            String hint,
            String accessibilityHint,
            /* SuggestTemplateInfo.TemplateAction.ActionType */ int actionType,
            String actionUri,
            int tabId,
            boolean showAsActionButton) {
        if (actionType == SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE
                && !mDialerAvailable) {
            return null;
        }
        return new OmniboxActionInSuggest(
                nativeInstance,
                hint,
                accessibilityHint,
                actionType,
                actionUri,
                tabId,
                showAsActionButton);
    }
}
