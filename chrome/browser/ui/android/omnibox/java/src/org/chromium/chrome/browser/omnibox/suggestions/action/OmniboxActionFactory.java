// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxPedalId;

/** A factory creating the OmniboxAction instances. */
@NullMarked
public class OmniboxActionFactory {
    private static boolean sDialerAvailable;

    /** Private constructor to suppress direct instantiation of this class. */
    private OmniboxActionFactory() {}

    /** Initialize the factory. Called before native code is ready. */
    public static void setDialerAvailable(boolean dialerAvailable) {
        sDialerAvailable = dialerAvailable;
    }

    @CalledByNative
    public static @Nullable OmniboxAction buildOmniboxPedal(
            long nativeInstance,
            String hint,
            String accessibilityHint,
            @OmniboxPedalId int pedalId) {
        return new OmniboxPedal(nativeInstance, hint, accessibilityHint, pedalId);
    }

    @CalledByNative
    public static @Nullable OmniboxAction buildSiteSearchAction(
            long nativeInstance,
            @JniType("std::u16string") String hint,
            @JniType("std::u16string") String accessibilityHint,
            @JniType("std::u16string") String keyword) {
        return new SiteSearchAction(nativeInstance, hint, accessibilityHint, keyword);
    }

    @CalledByNative
    public static @Nullable OmniboxAction buildActionInSuggest(
            long nativeInstance,
            String hint,
            String accessibilityHint,
            /* SuggestTemplateInfo.TemplateAction.ActionType */ int actionType,
            String actionUri,
            int tabId,
            @ActionPresentationMode int presentationMode) {
        if (actionType == SuggestTemplateInfo.TemplateAction.ActionType.CALL_VALUE
                && !sDialerAvailable) {
            return null;
        }

        return new OmniboxActionInSuggest(
                nativeInstance,
                hint,
                accessibilityHint,
                actionType,
                actionUri,
                tabId,
                presentationMode);
    }
}
