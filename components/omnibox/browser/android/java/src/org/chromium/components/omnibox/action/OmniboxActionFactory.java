// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.omnibox.EntityInfoProto;

/**
 * An interface for handling interactions for Omnibox Action Chips.
 */
public class OmniboxActionFactory {
    @CalledByNative
    public static @Nullable OmniboxAction buildOmniboxPedal(
            @NonNull String hint, @OmniboxPedalType int pedalId) {
        return new OmniboxPedal(hint, pedalId);
    }

    @CalledByNative
    public static @Nullable OmniboxActionInSuggest buildActionInSuggest(
            @NonNull String hint, @NonNull byte[] serializedActionInfo) {
        try {
            return new OmniboxActionInSuggest(
                    hint, EntityInfoProto.ActionInfo.parseFrom(serializedActionInfo));
        } catch (InvalidProtocolBufferException e) {
        }
        return null;
    }

    @CalledByNative
    public static @Nullable OmniboxAction buildHistoryClustersAction(
            @NonNull String hint, @NonNull String query) {
        return new HistoryClustersAction(hint, query);
    }
}
