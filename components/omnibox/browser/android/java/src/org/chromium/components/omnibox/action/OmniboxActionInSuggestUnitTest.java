// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.EntityInfoProto;

import java.util.List;

/**
 * Tests for {@link OmniboxActionInSuggest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxActionInSuggestUnitTest {
    private static final List<Integer> sKnownActionTypes =
            List.of(EntityInfoProto.ActionInfo.ActionType.CALL_VALUE,
                    EntityInfoProto.ActionInfo.ActionType.DIRECTIONS_VALUE,
                    EntityInfoProto.ActionInfo.ActionType.WEBSITE_VALUE);
    private static final EntityInfoProto.ActionInfo EMPTY_INFO =
            EntityInfoProto.ActionInfo.getDefaultInstance();

    @Test
    public void creation_usesCustomIconForKnownActionTypes() {
        for (var kesemActionType : sKnownActionTypes) {
            var proto = EntityInfoProto.ActionInfo.newBuilder()
                                .setActionType(EntityInfoProto.ActionInfo.ActionType.forNumber(
                                        kesemActionType))
                                .build();

            var action = new OmniboxActionInSuggest("hint", proto);
            assertNotEquals(OmniboxAction.DEFAULT_ICON, action.icon);
        }
    }

    @Test
    public void creation_usesFallbackIconForUnknownActionTypes() {
        for (var kesemActionType : EntityInfoProto.ActionInfo.ActionType.values()) {
            if (sKnownActionTypes.contains(kesemActionType.getNumber())) continue;

            var proto =
                    EntityInfoProto.ActionInfo.newBuilder().setActionType(kesemActionType).build();

            var action = new OmniboxActionInSuggest("hint", proto);
            assertEquals(OmniboxAction.DEFAULT_ICON, action.icon);
        }
    }

    @Test
    public void creation_creationFailsWithInvalidSerializedProto() {
        assertNull(OmniboxActionInSuggest.build("hint", new byte[] {1, 2, 3}));
    }

    @Test
    public void creation_creationSucceedsWithValidSerializedProto() {
        var proto = EntityInfoProto.ActionInfo.newBuilder().setDisplayedText("text").build();
        var action = OmniboxActionInSuggest.build("hint", proto.toByteArray());

        assertNotNull(action);
        assertEquals(action.actionInfo.getDisplayedText(), "text");
    }

    @Test
    public void creation_failsWithNullHint() {
        assertThrows(AssertionError.class, () -> new OmniboxActionInSuggest(null, EMPTY_INFO));
    }

    @Test
    public void creation_failsWithEmptyHint() {
        assertThrows(AssertionError.class, () -> new OmniboxActionInSuggest("", EMPTY_INFO));
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> OmniboxActionInSuggest.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(AssertionError.class,
                ()
                        -> OmniboxActionInSuggest.from(new OmniboxAction(
                                OmniboxActionType.ACTION_IN_SUGGEST, "hint", null)));
    }

    @Test
    public void safeCasting_successWithHistoryClusters() {
        OmniboxActionInSuggest.from(new OmniboxActionInSuggest("hint", EMPTY_INFO));
    }
}
