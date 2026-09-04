// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BottomSheetType}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetTypeUnitTest {

    @Test
    public void testDefaults() {
        BottomSheetType type = new BottomSheetType.Builder().build();
        assertFalse("userInitiated should default to false", type.isUserInitiated());
        assertTrue("modal should default to true", type.isModal());
        assertFalse("suppressible should default to false", type.isSuppressible());
        assertFalse("persistent should default to false", type.isPersistent());
        assertFalse("userCritical should default to false", type.isUserCritical());
    }

    @Test
    public void testPredefinedTypes() {
        assertFalse(BottomSheetType.Type.IPH.isUserInitiated());
        assertFalse(BottomSheetType.Type.IPH.isModal());
        assertTrue(BottomSheetType.Type.IPH.isSuppressible());
        assertFalse(BottomSheetType.Type.IPH.isPersistent());
        assertFalse(BottomSheetType.Type.IPH.isUserCritical());
    }

    @Test
    public void testCustomValues() {
        BottomSheetType type =
                new BottomSheetType.Builder()
                        .setUserInitiated(true)
                        .setModal(false)
                        .setSuppressible(true)
                        .setPersistent(true)
                        .setUserCritical(UserCriticalFeature.TEST)
                        .build();

        assertTrue(type.isUserInitiated());
        assertFalse(type.isModal());
        assertTrue(type.isSuppressible());
        assertTrue(type.isPersistent());
        assertTrue(type.isUserCritical());
    }

    @Test
    public void testEqualsAndHashCode() {
        BottomSheetType type1 =
                new BottomSheetType.Builder()
                        .setUserInitiated(true)
                        .setModal(true)
                        .setSuppressible(false)
                        .setPersistent(false)
                        .setUserCritical(UserCriticalFeature.TEST)
                        .build();

        BottomSheetType type2 =
                new BottomSheetType.Builder()
                        .setUserInitiated(true)
                        .setModal(true)
                        .setSuppressible(false)
                        .setPersistent(false)
                        .setUserCritical(UserCriticalFeature.TEST)
                        .build();

        BottomSheetType type3 =
                new BottomSheetType.Builder()
                        .setUserInitiated(false)
                        .setModal(true)
                        .setSuppressible(false)
                        .setPersistent(false)
                        .setUserCritical(UserCriticalFeature.TEST)
                        .build();

        BottomSheetType type4 =
                new BottomSheetType.Builder()
                        .setUserInitiated(true)
                        .setModal(true)
                        .setSuppressible(false)
                        .setPersistent(true)
                        .setUserCritical(UserCriticalFeature.TEST)
                        .build();

        assertEquals(type1, type2);
        assertEquals(type1.hashCode(), type2.hashCode());
        assertNotEquals(type1, type3);
        assertNotEquals(type1, type4);
    }

    @Test
    public void testCanSupersede_sameOrEqualType() {
        BottomSheetType type =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();

        BottomSheetType identicalType =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();

        assertFalse("Same instance should not supersede", type.canSupersede(type));
        assertFalse("Equal type should not supersede", type.canSupersede(identicalType));
    }

    @Test
    public void testCanSupersede_suppressibleCurrent() {
        BottomSheetType suppressibleCurrent =
                new BottomSheetType.Builder()
                        .setSuppressible(true)
                        .setUserCritical(UserCriticalFeature.TEST)
                        .setUserInitiated(true)
                        .setModal(true)
                        .build();

        BottomSheetType lowPriorityIncoming =
                new BottomSheetType.Builder().setUserInitiated(false).setModal(false).build();

        assertTrue(
                "Suppressible current sheet should always be superseded",
                lowPriorityIncoming.canSupersede(suppressibleCurrent));
    }

    @Test
    public void testCanSupersede_userCritical() {
        BottomSheetType criticalIncoming =
                new BottomSheetType.Builder()
                        .setUserCritical(UserCriticalFeature.TEST)
                        .setUserInitiated(false)
                        .setModal(false)
                        .build();

        BottomSheetType nonCriticalCurrent =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();

        assertTrue(
                "Critical incoming should supersede non-critical current",
                criticalIncoming.canSupersede(nonCriticalCurrent));
        assertFalse(
                "Non-critical incoming should not supersede critical current",
                nonCriticalCurrent.canSupersede(criticalIncoming));

        // Both user-critical: falls through to userInitiated check.
        BottomSheetType criticalUserInitiated =
                new BottomSheetType.Builder()
                        .setUserCritical(UserCriticalFeature.TEST)
                        .setUserInitiated(true)
                        .build();

        BottomSheetType criticalPassive =
                new BottomSheetType.Builder()
                        .setUserCritical(UserCriticalFeature.TEST)
                        .setUserInitiated(false)
                        .build();

        assertTrue(
                "Both critical: user-initiated incoming supersedes passive current",
                criticalUserInitiated.canSupersede(criticalPassive));
        assertFalse(
                "Both critical: passive incoming does not supersede user-initiated current",
                criticalPassive.canSupersede(criticalUserInitiated));
    }

    @Test
    public void testCanSupersede_userInitiated() {
        BottomSheetType userInitiatedIncoming =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(false).build();

        BottomSheetType passiveCurrent =
                new BottomSheetType.Builder().setUserInitiated(false).setModal(true).build();

        assertTrue(
                "User-initiated incoming should supersede passive current",
                userInitiatedIncoming.canSupersede(passiveCurrent));
        assertFalse(
                "Passive incoming should not supersede user-initiated current",
                passiveCurrent.canSupersede(userInitiatedIncoming));

        // Both user-initiated: incoming supersedes.
        BottomSheetType userInitiatedModal =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();

        BottomSheetType userInitiatedNonModal =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(false).build();

        assertTrue(
                "Both user-initiated: incoming supersedes current",
                userInitiatedNonModal.canSupersede(userInitiatedModal));
        assertTrue(
                "Both user-initiated: incoming supersedes current",
                userInitiatedModal.canSupersede(userInitiatedNonModal));
    }

    @Test
    public void testCanSupersede_persistent() {
        BottomSheetType persistentIncoming =
                new BottomSheetType.Builder()
                        .setUserInitiated(false)
                        .setPersistent(true)
                        .setModal(false)
                        .build();

        BottomSheetType nonPersistentCurrent =
                new BottomSheetType.Builder()
                        .setUserInitiated(false)
                        .setPersistent(false)
                        .setModal(true)
                        .build();

        assertTrue(
                "Persistent incoming should supersede non-persistent current",
                persistentIncoming.canSupersede(nonPersistentCurrent));
        assertFalse(
                "Non-persistent incoming should not supersede persistent current",
                nonPersistentCurrent.canSupersede(persistentIncoming));

        // Both persistent: falls through to modal check.
        BottomSheetType persistentModal =
                new BottomSheetType.Builder()
                        .setUserInitiated(false)
                        .setPersistent(true)
                        .setModal(true)
                        .build();

        assertTrue(
                "Both persistent: modal incoming supersedes non-modal current",
                persistentModal.canSupersede(persistentIncoming));
    }

    @Test
    public void testCanSupersede_modal() {
        BottomSheetType modalIncoming =
                new BottomSheetType.Builder().setUserInitiated(false).setModal(true).build();

        BottomSheetType nonModalCurrent =
                new BottomSheetType.Builder().setUserInitiated(false).setModal(false).build();

        assertTrue(
                "Modal incoming should supersede non-modal current",
                modalIncoming.canSupersede(nonModalCurrent));
        assertFalse(
                "Non-modal incoming should not supersede modal current",
                nonModalCurrent.canSupersede(modalIncoming));

        // Both passive and non-modal: keep current (incoming cannot supersede).
        BottomSheetType passiveNonModalIncoming =
                new BottomSheetType.Builder().setUserInitiated(false).setModal(false).build();

        assertFalse(
                "Both non-modal & passive: keep current sheet",
                passiveNonModalIncoming.canSupersede(nonModalCurrent));
    }

    @Test
    public void testCompare() {
        BottomSheetType higherPrecedence =
                new BottomSheetType.Builder().setUserInitiated(true).build();
        BottomSheetType lowerPrecedence =
                new BottomSheetType.Builder().setUserInitiated(false).build();

        assertEquals(-1, BottomSheetType.compare(higherPrecedence, lowerPrecedence));
        assertEquals(1, BottomSheetType.compare(lowerPrecedence, higherPrecedence));
        assertEquals(0, BottomSheetType.compare(higherPrecedence, higherPrecedence));
    }
}
