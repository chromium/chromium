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
    public void testRequestShow_noPriorPendingActions_addsShowOnly() {
        // Act.
        mManager.requestAction(PendingAction.SHOW);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals("Primary action should be SHOW.", PendingAction.SHOW, pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
    }

    // Examples:
    // Request: ACTIVATE->SHOW, Result: SHOW.
    // Request: DEACTIVATE->SHOW, Result: SHOW.
    @Test
    public void testRequestShow_afterSingleLowerPrecedenceAction_clearsActionAndAddsShow() {
        @PendingAction
        int[] lowerPrecedenceActions =
                new int[] {
                    PendingAction.HIDE,
                    PendingAction.SHOW_INACTIVE,
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
    public void testRequestShow_afterSingleHigherPrecedenceAction_ignoresShowAndRetainsOther() {
        @PendingAction
        int[] higherPrecedenceActions =
                new int[] {
                    PendingAction.CLOSE,
                    PendingAction.MAXIMIZE,
                    PendingAction.RESTORE,
                    PendingAction.SET_BOUNDS
                };

        doTestActionRetainsHigherPrecedenceAction(PendingAction.SHOW, higherPrecedenceActions);
    }

    // Examples:
    // Request: MAXIMIZE->DEACTIVATE->SHOW, Result: MAXIMIZE.
    // Request: SET_BOUNDS->SHOW_INACTIVE->SHOW, Result: SET_BOUNDS.
    @Test
    public void
            testRequestShow_afterTwoPendingActions_withHigherPrecedencePrimaryAction_retainsOtherPrimaryActionOnly() {
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
    public void testRequestSetBounds_withNonEmptyBounds_noPriorPendingActions_addsSetBoundsOnly() {
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
    public void testRequestSetBounds_withEmptyBounds_noPriorPendingActions_ignoresSetBounds() {
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
    public void testRequestSetBounds_afterSinglePendingAction_addsSetBoundsOnly() {
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

    @Test
    public void testRequestShowInactive_noPriorPendingActions_addsShowInactiveOnly() {
        // Act.
        mManager.requestAction(PendingAction.SHOW_INACTIVE);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Secondary action should be SHOW_INACTIVE.",
                PendingAction.SHOW_INACTIVE,
                pendingActions[1]);
        assertEquals("Primary action should be NONE.", PendingAction.NONE, pendingActions[0]);
    }

    // Examples:
    // Request: MINIMIZE->SHOW_INACTIVE, Result: SHOW_INACTIVE.
    // Request: DEACTIVATE->SHOW_INACTIVE, Result: SHOW_INACTIVE.
    @Test
    public void
            testRequestShowInactive_afterSingleLowerPrecedenceAction_clearsActionAndAddsShowInactive() {
        @PendingAction
        int[] lowerPrecedenceActions =
                new int[] {
                    PendingAction.SHOW,
                    PendingAction.HIDE,
                    PendingAction.ACTIVATE,
                    PendingAction.DEACTIVATE,
                    PendingAction.MINIMIZE
                };

        doTestActionOverridesLowerPrecedenceAction(
                PendingAction.SHOW_INACTIVE, lowerPrecedenceActions);
    }

    @Test
    public void testRequestShowInactive_afterClose_ignoresShowInactive() {
        // Arrange.
        mManager.setActionForTesting(PendingAction.CLOSE);

        // Act.
        mManager.requestAction(PendingAction.SHOW_INACTIVE);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals("Primary action should be CLOSE.", PendingAction.CLOSE, pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
    }

    // Examples:
    // Request: SET_BOUNDS->SHOW_INACTIVE, Result: SET_BOUNDS->SHOW_INACTIVE.
    // Request: MAXIMIZE->SHOW_INACTIVE, Result: MAXIMIZE->SHOW_INACTIVE.
    @Test
    public void testRequestShowInactive_afterSingleHigherPrecedenceAction_addsBothActions() {
        @PendingAction
        int[] higherPrecedenceActions =
                new int[] {PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS};

        doTestActionRetainsHigherPrecedenceAction(
                PendingAction.SHOW_INACTIVE, higherPrecedenceActions);
    }

    @Test
    public void testRequestActivate_noPriorPendingActions_addsActivateOnly() {
        // Act.
        mManager.requestAction(PendingAction.ACTIVATE);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Primary action should be ACTIVATE.", PendingAction.ACTIVATE, pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
    }

    // Examples:
    // Request: MINIMIZE->ACTIVATE, Result: ACTIVATE.
    // Request: DEACTIVATE->ACTIVATE, Result: ACTIVATE.
    @Test
    public void testRequestActivate_afterSingleLowerPrecedenceAction_clearsActionAndAddsActivate() {
        @PendingAction
        int[] lowerPrecedenceActions =
                new int[] {
                    PendingAction.HIDE,
                    PendingAction.SHOW_INACTIVE,
                    PendingAction.DEACTIVATE,
                    PendingAction.MINIMIZE
                };

        doTestActionOverridesLowerPrecedenceAction(PendingAction.ACTIVATE, lowerPrecedenceActions);
    }

    // Examples:
    // Request: CLOSE->ACTIVATE, Result: CLOSE.
    // Request: MAXIMIZE->ACTIVATE, Result: MAXIMIZE.
    @Test
    public void
            testRequestActivate_afterSingleHigherPrecedenceAction_ignoresActivateAndRetainsOther() {
        @PendingAction
        int[] higherPrecedenceActions =
                new int[] {
                    PendingAction.SHOW,
                    PendingAction.CLOSE,
                    PendingAction.MAXIMIZE,
                    PendingAction.RESTORE,
                    PendingAction.SET_BOUNDS
                };

        doTestActionRetainsHigherPrecedenceAction(PendingAction.ACTIVATE, higherPrecedenceActions);
    }

    // Examples:
    // Request: MAXIMIZE->DEACTIVATE->ACTIVATE, Result: MAXIMIZE.
    // Request: SET_BOUNDS->SHOW_INACTIVE->ACTIVATE, Result: SET_BOUNDS.
    @Test
    public void
            testRequestActivate_afterTwoPendingActions_withHigherPrecedencePrimaryAction_retainsOtherPrimaryActionOnly() {
        @PendingAction
        int[] secondaryActions = new int[] {PendingAction.SHOW_INACTIVE, PendingAction.DEACTIVATE};
        @PendingAction
        int[] higherPrecedenceActions =
                new int[] {PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS};

        for (@PendingAction int secondaryAction : secondaryActions) {
            mManager.setActionForTesting(secondaryAction);
            doTestActionRetainsHigherPrecedenceAction(
                    PendingAction.ACTIVATE, higherPrecedenceActions);
        }
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
            if (PendingActionManager.isPrimaryAction(action)) {
                assertEquals("Primary action should be " + action + ".", action, pendingActions[0]);
                assertEquals(
                        "Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
            } else {
                assertEquals(
                        "Secondary action should be " + action + ".", action, pendingActions[1]);
                assertEquals(
                        "Primary action should be NONE.", PendingAction.NONE, pendingActions[0]);
            }

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
            if (PendingActionManager.isPrimaryAction(action)) {
                assertEquals(
                        "Action should be ignored.", higherPrecedenceAction, pendingActions[0]);
                assertEquals(
                        "Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
            } else {
                assertEquals(
                        "Primary action should be " + higherPrecedenceAction + ".",
                        higherPrecedenceAction,
                        pendingActions[0]);
                assertEquals(
                        "Secondary action should be " + action + ".", action, pendingActions[1]);
            }
        }
    }
}
