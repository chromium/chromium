// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.omnibox.EntityInfoProto;
import org.chromium.components.omnibox.R;

/**
 * Omnibox action for showing the Action in Suggest UI.
 */
public class OmniboxActionInSuggest extends OmniboxAction {
    /** Map of {@link EntityInfoProto.ActionInfo.ActionType} to {@link ChipIcon}. */
    private static final SparseArray<ChipIcon> ICON_MAP = createIconMap();
    /** The details about the underlying action. */
    public final @NonNull EntityInfoProto.ActionInfo actionInfo;

    public OmniboxActionInSuggest(
            @NonNull String hint, @NonNull EntityInfoProto.ActionInfo actionInfo) {
        super(OmniboxActionType.ACTION_IN_SUGGEST, hint,
                ICON_MAP.get(actionInfo.getActionType().getNumber(), null));
        this.actionInfo = actionInfo;
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

    /** Returns a map of ActionType to ChipIcon. */
    private static SparseArray<ChipIcon> createIconMap() {
        var map = new SparseArray<ChipIcon>();
        map.put(EntityInfoProto.ActionInfo.ActionType.CALL_VALUE,
                new ChipIcon(R.drawable.action_call, true));
        map.put(EntityInfoProto.ActionInfo.ActionType.DIRECTIONS_VALUE,
                new ChipIcon(R.drawable.action_directions, true));
        map.put(EntityInfoProto.ActionInfo.ActionType.WEBSITE_VALUE,
                new ChipIcon(R.drawable.action_web, true));
        return map;
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
