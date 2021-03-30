// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.IntDef;

/**
 * The class describing the change of message scope, including the target scope type, instance id,
 * the {@link ChangeType} and the {@link Priority}.
 * The values of scope type and scope instance ids are maintained by the clients.
 */
public class MessageScopeChange {
    @IntDef({ChangeType.ACTIVE, ChangeType.INACTIVE, ChangeType.DESTROY})
    public @interface ChangeType {
        int ACTIVE = 0;
        int INACTIVE = 1;
        int DESTROY = 2;
    }

    public final int scopeTypeId;
    public final @ChangeType int changeType;
    public final ScopeKey scopeInstanceKey;
    public final boolean animateTransition;

    /**
     * @param scopeTypeId The {@link MessageScopeType} indicating the type of scope.
     * @param scopeInstanceKey An identical object as a key of Scope Instance.
     * @param changeType The {@link ChangeType} indicating the type of change.
     */
    public MessageScopeChange(@MessageScopeType int scopeTypeId, ScopeKey scopeInstanceKey,
            @ChangeType int changeType) {
        this(scopeTypeId, scopeInstanceKey, changeType, true);
    }

    /**
     * @param scopeTypeId The {@link MessageScopeType} indicating the type of scope.
     * @param scopeInstanceKey An identical object as a key of Scope Instance.
     * @param changeType The {@link ChangeType} indicating the type of change.
     * @param animateTransition Whether animation should be shown to reflect the scope change.
     */
    public MessageScopeChange(@MessageScopeType int scopeTypeId, ScopeKey scopeInstanceKey,
            @ChangeType int changeType, boolean animateTransition) {
        this.scopeTypeId = scopeTypeId;
        this.scopeInstanceKey = scopeInstanceKey;
        this.changeType = changeType;
        this.animateTransition = animateTransition;
    }
}
