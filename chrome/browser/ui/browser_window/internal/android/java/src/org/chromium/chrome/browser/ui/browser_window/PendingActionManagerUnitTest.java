// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;

/** Unit tests for {@link PendingActionManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PendingActionManagerUnitTest {
    private PendingActionManager mManager;

    @Before
    public void setUp() {
        mManager = new PendingActionManager();
    }

    @Test
    public void testRequestShow_noPendingActions() {
        // Act.
        mManager.requestAction(PendingAction.SHOW);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals("Primary action should be SHOW.", PendingAction.SHOW, pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
    }

    // Examples:
    // Request: CLOSE->SHOW, Result: SHOW.
    // Request: DEACTIVATE->SHOW, Result: SHOW.
    @Test
    public void testRequestShow_afterSingleLowerPrecedenceAction() {
        @PendingAction
        int[] lowerPrecedenceActions =
                new int[] {
                    PendingAction.HIDE,
                    PendingAction.SHOW_INACTIVE,
                    PendingAction.CLOSE,
                    PendingAction.ACTIVATE,
                    PendingAction.DEACTIVATE,
                    PendingAction.MINIMIZE
                };

        doTestActionOverridesLowerPrecedenceAction(PendingAction.SHOW, lowerPrecedenceActions);
    }

    // Examples:
    // Request: SET_BOUNDS->SHOW, Result: SET_BOUNDS.
    // Request: MAXIMIZE->SHOW, Result: MAXIMIZE.
    @Test
    public void testRequestShow_afterSingleHigherPrecedenceAction() {
        @PendingAction
        int[] higherPrecedenceActions =
                new int[] {PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS};

        doTestActionRetainsHigherPrecedenceAction(PendingAction.SHOW, higherPrecedenceActions);
    }

    // Examples:
    // Request: MAXIMIZE->DEACTIVATE->SHOW, Result: MAXIMIZE.
    // Request: SET_BOUNDS->SHOW_INACTIVE->SHOW, Result: SET_BOUNDS.
    @Test
    public void testRequestShow_afterTwoPendingActions_withHigherPrecedencePrimaryAction() {
        @PendingAction
        int[] secondaryActions = new int[] {PendingAction.SHOW_INACTIVE, PendingAction.DEACTIVATE};
        @PendingAction
        int[] higherPrecedenceActions =
                new int[] {PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS};

        for (@PendingAction int secondaryAction : secondaryActions) {
            mManager.setActionForTesting(secondaryAction);
            doTestActionRetainsHigherPrecedenceAction(PendingAction.SHOW, higherPrecedenceActions);
        }
    }

    @Test
    public void testRequestSetBounds_noPendingActions_withNonEmptyBounds() {
        // Arrange.
        Rect bounds = new Rect(0, 0, 100, 100);

        // Act.
        mManager.requestSetBounds(bounds);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Primary action should be SET_BOUNDS.",
                PendingAction.SET_BOUNDS,
                pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        assertEquals("Bounds should be saved.", bounds, mManager.getPendingBoundsForTesting());
    }

    @Test
    public void testRequestSetBounds_withEmptyBounds() {
        // Act.
        mManager.requestSetBounds(new Rect());

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals("Primary action should be NONE.", PendingAction.NONE, pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        assertNull("Bounds should not be saved.", mManager.getPendingBoundsForTesting());
    }

    @Test
    public void testRequestSetBounds_duplicateRequestOverridesBounds() {
        // Arrange.
        Rect bounds1 = new Rect(0, 0, 100, 100);
        Rect bounds2 = new Rect(0, 0, 200, 200);

        // Act.
        mManager.requestSetBounds(bounds1);
        mManager.requestSetBounds(bounds2);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Primary action should be SET_BOUNDS.",
                PendingAction.SET_BOUNDS,
                pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        assertEquals("Bounds should be updated.", bounds2, mManager.getPendingBoundsForTesting());
    }

    @Test
    public void testRequestSetBounds_afterSinglePendingAction() {
        Rect bounds = new Rect(0, 0, 100, 100);
        @PendingAction
        int[] lowerPrecedenceActions =
                new int[] {
                    PendingAction.SHOW,
                    PendingAction.HIDE,
                    PendingAction.SHOW_INACTIVE,
                    PendingAction.CLOSE,
                    PendingAction.ACTIVATE,
                    PendingAction.DEACTIVATE,
                    PendingAction.MINIMIZE,
                    PendingAction.MAXIMIZE,
                    PendingAction.RESTORE,
                };
        doTestActionOverridesLowerPrecedenceAction(
                PendingAction.SET_BOUNDS, lowerPrecedenceActions, bounds);
    }

    private void doTestActionOverridesLowerPrecedenceAction(
            @PendingAction int action, @PendingAction int[] lowerPrecedenceActions) {
        doTestActionOverridesLowerPrecedenceAction(
                action, lowerPrecedenceActions, /* bounds= */ null);
    }

    private void doTestActionOverridesLowerPrecedenceAction(
            @PendingAction int action, @PendingAction int[] lowerPrecedenceActions, Rect bounds) {
        for (@PendingAction int lowerPrecedenceAction : lowerPrecedenceActions) {
            // Arrange.
            mManager.clearPendingActionsForTesting();
            mManager.setActionForTesting(lowerPrecedenceAction);

            // Act.
            if (action == PendingAction.SET_BOUNDS) {
                mManager.requestSetBounds(bounds);
            } else {
                mManager.requestAction(action);
            }

            // Assert.
            var pendingActions = mManager.getPendingActionsForTesting();
            assertEquals("Primary action should be " + action + ".", action, pendingActions[0]);
            assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
            if (action == PendingAction.SET_BOUNDS) {
                assertEquals(
                        "Bounds should be saved.", bounds, mManager.getPendingBoundsForTesting());
            }
        }
    }

    private void doTestActionRetainsHigherPrecedenceAction(
            @PendingAction int action, @PendingAction int[] higherPrecedenceActions) {
        for (@PendingAction int higherPrecedenceAction : higherPrecedenceActions) {
            // Arrange.
            mManager.setActionForTesting(higherPrecedenceAction);

            // Act.
            mManager.requestAction(action);

            // Assert.
            var pendingActions = mManager.getPendingActionsForTesting();
            assertEquals("Action should be ignored.", higherPrecedenceAction, pendingActions[0]);
            assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        }
    }
}
