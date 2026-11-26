// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.fusebox.Fusebox.PopupState;
import org.chromium.components.omnibox.OmniboxFocusReason;

/** Unit tests for {@link FuseboxGuidedTourController}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class FuseboxGuidedTourControllerUnitTest {
    private final SettableNonNullObservableSupplier<@PopupState Integer> mPopupStateSupplier =
            ObservableSuppliers.createNonNull(PopupState.HIDDEN);

    private FuseboxGuidedTourController mController;

    @Before
    public void setUp() {
        mController = new FuseboxGuidedTourController(mPopupStateSupplier);
    }

    @Test
    public void beginInput_nonGuidedTourFocusReason_staysDisabled() {
        mController.beginInput(OmniboxFocusReason.FAKE_BOX_TAP);

        assertFalse(mController.isGuidedTourModeActive());
    }

    @Test
    public void beginInput_guidedTourFocusReason_enablesMode() {
        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);

        assertTrue(mController.isGuidedTourModeActive());
    }

    @Test
    public void beginInput_nonGuidedTourAfterTour_exitsMode() {
        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);

        mController.beginInput(OmniboxFocusReason.FAKE_BOX_TAP);

        assertFalse(mController.isGuidedTourModeActive());
    }

    @Test
    public void endInput_exitsMode() {
        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);

        mController.endInput();

        assertFalse(mController.isGuidedTourModeActive());
    }

    @Test
    public void popupStateChanged_hidden_exitsMode() {
        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);
        mPopupStateSupplier.set(PopupState.FLOATING);
        assertTrue(mController.isGuidedTourModeActive());

        mPopupStateSupplier.set(PopupState.HIDDEN);

        assertFalse(mController.isGuidedTourModeActive());
    }

    @Test
    public void destroy_exitsMode() {
        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);

        mController.destroy();

        assertFalse(mController.isGuidedTourModeActive());
    }

    @Test
    public void activeTour_attachesObserver() {
        assertFalse(mPopupStateSupplier.hasObservers());

        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);

        assertTrue(mPopupStateSupplier.hasObservers());
    }

    @Test
    public void inactiveTour_detachesObserver() {
        mController.beginInput(OmniboxFocusReason.FUSEBOX_GUIDED_TOUR);

        mController.endInput();

        assertFalse(mPopupStateSupplier.hasObservers());
    }
}
