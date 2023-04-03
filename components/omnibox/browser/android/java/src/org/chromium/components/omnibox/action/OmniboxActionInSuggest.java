// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.components.omnibox.EntityInfoProto;

/**
 * Omnibox action for showing the Action in Suggest UI.
 */
public class OmniboxActionInSuggest extends OmniboxAction {
    /** The details about the underlying action. */
    public final @NonNull EntityInfoProto.ActionInfo actionInfo;

    public OmniboxActionInSuggest(
            @NonNull String hint, @NonNull EntityInfoProto.ActionInfo actionInfo) {
        super(OmniboxActionType.ACTION_IN_SUGGEST, hint);
        assert actionInfo != null;
        this.actionInfo = actionInfo;
    }

    @Override
    public @NonNull ChipIcon getIcon() {
        return new ChipIcon(R.drawable.fre_product_logo, /*tintWithTextColor=*/false);
    }

    /**
     * Cast supplied OmniboxAction to OmniboxActionInSuggest.
     * Requires the supplied input to be a valid instance of an OmniboxActionInSuggest whose
     * actionId is the ACTION_IN_SUGGEST.
     */
    public static @NonNull OmniboxActionInSuggest from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionType.ACTION_IN_SUGGEST;
        assert action instanceof OmniboxActionInSuggest;
        return (OmniboxActionInSuggest) action;
    }

    @CalledByNative
    @VisibleForTesting
    public static @Nullable OmniboxActionInSuggest build(
            @NonNull String hint, @NonNull byte[] serializedActionInfo) {
        try {
            return new OmniboxActionInSuggest(
                    hint, EntityInfoProto.ActionInfo.parseFrom(serializedActionInfo));
        } catch (InvalidProtocolBufferException e) {
        }
        return null;
    }
}
