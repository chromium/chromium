// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.graphics.Rect;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskImpl.State;
import org.chromium.chrome.browser.ui.browser_window.PendingActionManager.PendingAction;

/** Unit tests for {@link PendingActionManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PendingActionManagerUnitTest {
    private static final @PendingAction int[] ALL_ACTIONS = {
        PendingAction.SHOW,
        PendingAction.HIDE,
        PendingAction.SHOW_INACTIVE,
        PendingAction.CLOSE,
        PendingAction.ACTIVATE,
        PendingAction.DEACTIVATE,
        PendingAction.MAXIMIZE,
        PendingAction.MINIMIZE,
        PendingAction.RESTORE,
        PendingAction.SET_BOUNDS
    };

    private static final @PendingAction int[] NO_INPUT_GLOBAL_OVERRIDE_ACTIONS = {
        PendingAction.HIDE, PendingAction.CLOSE, PendingAction.MINIMIZE,
    };

    private static final @PendingAction int[] SECONDARY_ACTIONS = {
        PendingAction.SHOW_INACTIVE, PendingAction.DEACTIVATE
    };

    private static final Rect TEST_SET_BOUNDS_INPUT_1 = new Rect(0, 0, 100, 100);
    private static final Rect TEST_SET_BOUNDS_INPUT_2 = new Rect(0, 0, 200, 200);

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
        int[] lowerPrecedenceActions = {
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
        int[] higherPrecedenceActions = {
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
        doTestActionRequestedAfterTwoPendingActions(PendingAction.SHOW);
    }

    @Test
    public void testRequestSetBounds_withNonEmptyBounds_noPriorPendingActions_addsSetBoundsOnly() {
        // Act.
        mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Primary action should be SET_BOUNDS.",
                PendingAction.SET_BOUNDS,
                pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        assertEquals(
                "Bounds should be saved.",
                TEST_SET_BOUNDS_INPUT_1,
                mManager.getPendingBoundsInDp());
        assertEquals(
                "Restored bounds should be saved.",
                TEST_SET_BOUNDS_INPUT_1,
                mManager.getPendingRestoredBoundsInDp());
    }

    @Test
    public void testRequestSetBounds_withEmptyBounds_noPriorPendingActions_ignoresSetBounds() {
        // Act.
        mManager.requestSetBounds(new Rect());

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals("Primary action should be NONE.", PendingAction.NONE, pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        assertNull("Bounds should not be saved.", mManager.getPendingBoundsInDp());
    }

    @Test
    public void testRequestSetBounds_duplicateRequestOverridesBounds() {
        // Act.
        mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);
        mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_2);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Primary action should be SET_BOUNDS.",
                PendingAction.SET_BOUNDS,
                pendingActions[0]);
        assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        assertEquals(
                "Bounds should be updated.",
                TEST_SET_BOUNDS_INPUT_2,
                mManager.getPendingBoundsInDp());
        assertEquals(
                "Restored bounds should be updated.",
                TEST_SET_BOUNDS_INPUT_2,
                mManager.getPendingRestoredBoundsInDp());
    }

    @Test
    public void testRequestSetBounds_afterSinglePendingAction_addsSetBoundsOnly() {
        @PendingAction
        int[] lowerPrecedenceActions = {
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
                PendingAction.SET_BOUNDS, lowerPrecedenceActions, TEST_SET_BOUNDS_INPUT_1);
    }

    @Test
    public void testRequestSetBounds_afterTwoPendingActions_addsNewerBounds() {
        doTestGlobalOverrideActionRequestedAfterTwoPendingActions(PendingAction.SET_BOUNDS);
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
        int[] lowerPrecedenceActions = {
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
        mManager.requestAction(PendingAction.CLOSE);

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
        int[] higherPrecedenceActions = {
            PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS
        };

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
        int[] lowerPrecedenceActions = {
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
        int[] higherPrecedenceActions = {
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
        doTestActionRequestedAfterTwoPendingActions(PendingAction.ACTIVATE);
    }

    @Test
    public void testRequestDeactivate_noPriorPendingActions_addsDeactivateOnly() {
        // Act.
        mManager.requestAction(PendingAction.DEACTIVATE);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Secondary action should be DEACTIVATE.",
                PendingAction.DEACTIVATE,
                pendingActions[1]);
        assertEquals("Primary action should be NONE.", PendingAction.NONE, pendingActions[0]);
    }

    // Examples:
    // Request: SHOW->DEACTIVATE, Result: DEACTIVATE.
    // Request: ACTIVATE->DEACTIVATE, Result: DEACTIVATE.
    @Test
    public void
            testRequestDeactivate_afterSingleLowerPrecedenceAction_clearsActionAndAddsDeactivate() {
        @PendingAction int[] lowerPrecedenceActions = {PendingAction.SHOW, PendingAction.ACTIVATE};

        doTestActionOverridesLowerPrecedenceAction(
                PendingAction.DEACTIVATE, lowerPrecedenceActions);
    }

    // Examples:
    // Request: SET_BOUNDS->DEACTIVATE, Result: SET_BOUNDS->DEACTIVATE.
    // Request: MAXIMIZE->DEACTIVATE, Result: MAXIMIZE->DEACTIVATE.
    @Test
    public void testRequestDeactivate_afterSingleHigherPrecedenceAction_initiatesBothActions() {
        @PendingAction
        int[] higherPrecedenceActions = {
            PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS
        };

        doTestActionRetainsHigherPrecedenceAction(
                PendingAction.DEACTIVATE, higherPrecedenceActions);
    }

    // Examples:
    // Request: CLOSE->DEACTIVATE, Result: CLOSE.
    // Request: MINIMIZE->DEACTIVATE, Result: MINIMIZE.
    @Test
    public void
            testRequestDeactivate_afterSingleHigherPrecedenceAction_ignoresDeactivateAndRetainsOther() {
        @PendingAction
        int[] higherPrecedencePrimaryActions = {
            PendingAction.HIDE, PendingAction.CLOSE, PendingAction.MINIMIZE
        };

        for (@PendingAction int higherPrecedenceAction : higherPrecedencePrimaryActions) {
            // Arrange.
            mManager.requestAction(higherPrecedenceAction);

            // Act.
            mManager.requestAction(PendingAction.DEACTIVATE);

            // Assert.
            var pendingActions = mManager.getPendingActionsForTesting();
            assertEquals(
                    "Primary action should be " + higherPrecedenceAction + ".",
                    higherPrecedenceAction,
                    pendingActions[0]);
            assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
        }
    }

    @Test
    public void testRequestDeactivate_afterShowInactive_ignoresDeactivate() {
        // Arrange.
        mManager.requestAction(PendingAction.SHOW_INACTIVE);

        // Act.
        mManager.requestAction(PendingAction.DEACTIVATE);

        // Assert.
        var pendingActions = mManager.getPendingActionsForTesting();
        assertEquals(
                "Secondary action should be SHOW_INACTIVE.",
                PendingAction.SHOW_INACTIVE,
                pendingActions[1]);
    }

    @Test
    public void testRequestGlobalOverrideAction_noPriorPendingActions_addsSingleAction() {
        for (@PendingAction int action : NO_INPUT_GLOBAL_OVERRIDE_ACTIONS) {
            // Arrange.
            mManager.clearPendingActionsForTesting();

            // Act.
            mManager.requestAction(action);

            // Assert.
            var pendingActions = mManager.getPendingActionsForTesting();
            assertEquals("Primary action should be " + action + ".", action, pendingActions[0]);
            assertEquals("Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
            assertNull("Bounds should be cleared.", mManager.getPendingBoundsInDp());
        }
    }

    // Examples:
    // Request: MAXIMIZE->MINIMIZE, Result: MINIMIZE.
    // Request: DEACTIVATE->MAXIMIZE, Result: MAXIMIZE.
    @Test
    public void testRequestGlobalOverrideAction_afterSingleAction_overridesPriorAction() {
        for (@PendingAction int action : NO_INPUT_GLOBAL_OVERRIDE_ACTIONS) {
            doTestActionOverridesLowerPrecedenceAction(action, ALL_ACTIONS);
        }
    }

    // Examples:
    // Request: MAXIMIZE->DEACTIVATE->MINIMIZE, Result: MINIMIZE.
    // Request: SET_BOUNDS->SHOW_INACTIVE->CLOSE, Result: CLOSE.
    @Test
    public void testRequestGlobalOverrideAction_afterTwoPendingActions_overridesBothActions() {
        for (@PendingAction int globalOverrideAction : NO_INPUT_GLOBAL_OVERRIDE_ACTIONS) {
            doTestGlobalOverrideActionRequestedAfterTwoPendingActions(globalOverrideAction);
        }
    }

    @Test
    public void testIsActiveFuture_afterRequestActivate_returnsTrue() {
        // Arrange.
        mManager.requestAction(PendingAction.ACTIVATE);

        // Assert.
        assertEquals(
                "isActive should be true in the future when ACTIVATE is in progress",
                true,
                mManager.isActiveFuture(State.PENDING_CREATE));
    }

    @Test
    public void testIsVisibleFuture_afterRequestShow_returnsTrue() {
        // Arrange.
        mManager.requestAction(PendingAction.SHOW);

        // Assert.
        assertEquals(
                "isVisible should be true in the future when SHOW is in progress",
                true,
                mManager.isVisibleFuture(State.PENDING_UPDATE));
    }

    @Test
    public void testIsVisibleFuture_afterRequestMinimize_returnsFalse() {
        // Arrange.
        mManager.requestAction(PendingAction.MINIMIZE);

        // Assert.
        assertEquals(
                "isVisible should be false in the future when MINIMIZE is in progress",
                false,
                mManager.isVisibleFuture(State.PENDING_UPDATE));
    }

    @Test
    public void testIsMaximizedFuture_afterRequestMaximize_returnsTrue() {
        // Arrange.
        mManager.requestMaximize(new Rect());

        // Assert.
        assertEquals(
                "isMaximized should be true in the future when MAXIMIZE is in progress",
                true,
                mManager.isMaximizedFuture(State.PENDING_UPDATE));
    }

    @Test
    public void testIsActiveFuture_afterRequestMaximize_returnsTrue() {
        // Arrange.
        mManager.requestMaximize(new Rect());

        // Assert.
        assertEquals(
                "isActive should be true in the future when MAXIMIZE is in progress",
                true,
                mManager.isActiveFuture(State.PENDING_UPDATE));
    }

    @Test
    public void testSetBounds_afterSetBounds_returnsPendingBounds() {
        // Arrange.
        mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);

        // Assert.
        assertEquals(
                "Should return pending bounds",
                TEST_SET_BOUNDS_INPUT_1,
                mManager.getPendingBoundsInDp());
    }

    @Test
    public void testClearSetBounds_afterSetBounds_returnsNull() {
        // Arrange.
        mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);
        mManager.getAndClearTargetPendingActions(PendingAction.SET_BOUNDS);

        // Assert.
        assertNull("Pending bounds should have been clear", mManager.getFutureBoundsInDp());
    }

    @Test
    public void testGetAndClearTargetPendingActions_afterClear_stateReturnsNull() {
        // Arrange.
        mManager.requestAction(PendingAction.ACTIVATE);
        assertEquals(true, mManager.isActiveFuture(State.PENDING_UPDATE));

        mManager.getAndClearTargetPendingActions(PendingAction.ACTIVATE);
        assertNull(
                "No pending action affecting isActive's future state",
                mManager.isActiveFuture(State.PENDING_UPDATE));
    }

    private void doTestActionOverridesLowerPrecedenceAction(
            @PendingAction int action, @PendingAction int[] lowerPrecedenceActions) {
        doTestActionOverridesLowerPrecedenceAction(
                action, lowerPrecedenceActions, /* bounds= */ null);
    }

    private void doTestActionOverridesLowerPrecedenceAction(
            @PendingAction int action, @PendingAction int[] lowerPrecedenceActions, Rect bounds) {
        for (@PendingAction int lowerPrecedenceAction : lowerPrecedenceActions) {
            if (lowerPrecedenceAction == action) {
                continue;
            }

            // Arrange.
            mManager.clearPendingActionsForTesting();
            if (lowerPrecedenceAction == PendingAction.SET_BOUNDS) {
                mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);
            } else if (lowerPrecedenceAction == PendingAction.MAXIMIZE) {
                mManager.requestMaximize(new Rect());
            } else if (lowerPrecedenceAction == PendingAction.RESTORE) {
                mManager.requestRestore(new Rect());
            } else {
                mManager.requestAction(lowerPrecedenceAction);
            }

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

            if (lowerPrecedenceAction == PendingAction.SET_BOUNDS) {
                assertNull("Bounds should be cleared.", mManager.getPendingBoundsInDp());
                assertNotNull(
                        "Restored bounds should not be cleared.",
                        mManager.getPendingRestoredBoundsInDp());
            }

            if (action == PendingAction.SET_BOUNDS) {
                assertEquals("Bounds should be saved.", bounds, mManager.getPendingBoundsInDp());
                assertEquals(
                        "Restored bounds should be saved.",
                        bounds,
                        mManager.getPendingRestoredBoundsInDp());
            }
        }
    }

    private void doTestActionRetainsHigherPrecedenceAction(
            @PendingAction int action, @PendingAction int[] higherPrecedenceActions) {
        for (@PendingAction int higherPrecedenceAction : higherPrecedenceActions) {
            // Arrange.
            if (higherPrecedenceAction == PendingAction.SET_BOUNDS) {
                mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);
            } else if (higherPrecedenceAction == PendingAction.MAXIMIZE) {
                mManager.requestMaximize(new Rect());
            } else if (higherPrecedenceAction == PendingAction.RESTORE) {
                mManager.requestRestore(new Rect());
            } else {
                mManager.requestAction(higherPrecedenceAction);
            }

            // Act.
            mManager.requestAction(action);

            // Assert.
            var pendingActions = mManager.getPendingActionsForTesting();
            assertEquals(
                    "Primary action should be " + higherPrecedenceAction + ".",
                    higherPrecedenceAction,
                    pendingActions[0]);
            if (PendingActionManager.isPrimaryAction(action)) {
                assertEquals(
                        "Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
            } else {
                assertEquals(
                        "Secondary action should be " + action + ".", action, pendingActions[1]);
            }
        }
    }

    // This tests a request for a primary, non-global override action after primary and secondary
    // actions already exist. In this scenario, only the older primary action will be acknowledged.
    private void doTestActionRequestedAfterTwoPendingActions(@PendingAction int action) {
        @PendingAction
        int[] priorPrimaryActions = {
            PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS
        };

        for (@PendingAction int priorSecondaryAction : SECONDARY_ACTIONS) {
            for (@PendingAction int primaryAction : priorPrimaryActions) {
                // Arrange.
                if (primaryAction == PendingAction.SET_BOUNDS) {
                    mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);
                } else if (primaryAction == PendingAction.MAXIMIZE) {
                    mManager.requestMaximize(new Rect());
                } else {
                    Assert.assertEquals(PendingAction.RESTORE, primaryAction);
                    mManager.requestRestore(new Rect());
                }
                mManager.requestAction(priorSecondaryAction);

                // Act.
                mManager.requestAction(action);

                // Assert.
                var pendingActions = mManager.getPendingActionsForTesting();
                assertEquals(
                        "Primary action should be " + primaryAction + ".",
                        primaryAction,
                        pendingActions[0]);
                assertEquals(
                        "Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
                if (primaryAction == PendingAction.SET_BOUNDS) {
                    assertEquals(
                            "Bounds should be preserved.",
                            TEST_SET_BOUNDS_INPUT_1,
                            mManager.getPendingBoundsInDp());
                    assertEquals(
                            "Restored bounds should be preserved.",
                            TEST_SET_BOUNDS_INPUT_1,
                            mManager.getPendingRestoredBoundsInDp());
                }
            }
        }
    }

    // This tests a request for a global override action after primary and secondary actions
    // already exist. In this scenario, only the third request will be acknowledged.
    private void doTestGlobalOverrideActionRequestedAfterTwoPendingActions(
            @PendingAction int action) {
        @PendingAction
        int[] priorPrimaryActions = {
            PendingAction.MAXIMIZE, PendingAction.RESTORE, PendingAction.SET_BOUNDS
        };

        for (@PendingAction int priorSecondaryAction : SECONDARY_ACTIONS) {
            for (@PendingAction int priorPrimaryAction : priorPrimaryActions) {
                // Arrange.
                if (priorPrimaryAction == PendingAction.SET_BOUNDS) {
                    mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_1);
                } else if (priorPrimaryAction == PendingAction.MAXIMIZE) {
                    mManager.requestMaximize(new Rect());
                } else {
                    Assert.assertEquals(PendingAction.RESTORE, priorPrimaryAction);
                    mManager.requestRestore(new Rect());
                }

                mManager.requestAction(priorSecondaryAction);

                // Act.
                if (action == PendingAction.SET_BOUNDS) {
                    mManager.requestSetBounds(TEST_SET_BOUNDS_INPUT_2);
                } else {
                    mManager.requestAction(action);
                }

                // Assert.
                var pendingActions = mManager.getPendingActionsForTesting();
                assertEquals("Primary action should be " + action + ".", action, pendingActions[0]);
                assertEquals(
                        "Secondary action should be NONE.", PendingAction.NONE, pendingActions[1]);
                if (action == PendingAction.SET_BOUNDS) {
                    assertEquals(
                            "Bounds should be saved.",
                            TEST_SET_BOUNDS_INPUT_2,
                            mManager.getPendingBoundsInDp());
                    assertEquals(
                            "Restored bounds should be saved.",
                            TEST_SET_BOUNDS_INPUT_2,
                            mManager.getPendingRestoredBoundsInDp());
                }
            }
        }
    }
}
