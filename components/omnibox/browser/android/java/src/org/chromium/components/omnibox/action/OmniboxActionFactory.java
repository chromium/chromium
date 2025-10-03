// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** An interface for creation of the OmniboxAction instances. */
@NullMarked
public interface OmniboxActionFactory {
    /**
     * Create a new OmniboxPedal.
     *
     * @param hint the title displayed on the chip
     * @param accessibilityHint the text to be announced to the accessibility-enabled users
     * @param pedalId the specific kind of pedal to create
     * @return new instance of an OmniboxPedal
     */
    @CalledByNative
    @Nullable OmniboxAction buildOmniboxPedal(
            long instance, String hint, String accessibilityHint, @OmniboxPedalId int pedalId);

    /**
     * Create a new OmniboxActionInSuggest.
     *
     * @param hint the title displayed on the chip
     * @param accessibilityHint the text to be announced to the accessibility-enabled users
     * @param actionType the specific type of an action matching the {@link
     *     SuggestTemplateInfo.TemplateAction.ActionType}
     * @param actionUri the corresponding action URI/URL (serialized intent)
     * @param showAsActionButton whether to show it as action button
     * @return new instance of an OmniboxActionInSuggest
     */
    @CalledByNative
    @Nullable OmniboxAction buildActionInSuggest(
            long instance,
            String hint,
            String accessibilityHint,
            /* SuggestTemplateInfo.TemplateAction.ActionType */ int actionType,
            String actionUri,
            int tabId,
            boolean showAsActionButton);

    @NativeMethods
    public interface Natives {
        /** Pass the OmniboxActionFactory instance to C++. */
        void setFactory(@Nullable OmniboxActionFactory javaFactory);
    }
}
