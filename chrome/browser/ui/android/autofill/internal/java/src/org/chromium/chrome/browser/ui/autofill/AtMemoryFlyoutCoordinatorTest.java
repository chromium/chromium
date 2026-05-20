// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.List;

/** Component tests for the AtMemory Flyout Coordinator. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryFlyoutCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AtMemoryFlyoutCoordinator.Delegate mMockDelegate;

    private AtMemoryFlyoutCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator =
                new AtMemoryFlyoutCoordinator(
                        new ContextThemeWrapper(
                                ApplicationProvider.getApplicationContext(),
                                R.style.Theme_BrowserUI_DayNight),
                        mBottomSheetController,
                        mMockDelegate);
    }

    @Test
    public void testInitialization() {
        assertNotNull(mCoordinator);
    }

    @Test
    public void testShowRequestsBottomSheet() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);

        mCoordinator.show(new ArrayList<>());

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testShowFailsAndDismisses() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);

        mCoordinator.show(new ArrayList<>());

        verify(mMockDelegate).onDismissed();
    }

    // TODO(crbug.com/514270637): Decouple display title from raw value to prevent filling formatted
    // or cropped data into forms.
    @Test
    public void testChipClickNotifiesDelegate() {
        List<Pair<String, String>> chipsData = new ArrayList<>();
        chipsData.add(new Pair<>("Elisa Beckett", ""));
        chipsData.add(new Pair<>("123530", "Passport number"));
        chipsData.add(new Pair<>("07-05-2026", "Issue date"));
        chipsData.add(new Pair<>("07-05-2036", "Expiration date"));
        chipsData.add(new Pair<>("USA", ""));
        mCoordinator.show(chipsData);
        View view = mCoordinator.getViewForTesting();
        ViewGroup chipsContainer = view.findViewById(R.id.flyout_chips_container);

        int chipIndex = 0;
        for (int i = 0; i < chipsContainer.getChildCount(); i++) {
            View childView = chipsContainer.getChildAt(i);
            if (childView instanceof ChipView) {
                childView.performClick();
                verify(mMockDelegate).onChipClicked(eq(chipsData.get(chipIndex).first));
                chipIndex++;
            }
        }
        assertEquals(chipsData.size(), chipIndex);
    }
}
