// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

/** Unit tests for {@link FuseboxSessionState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxSessionStateUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock Profile mProfile;
    private @Mock ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    private @Mock AutocompleteController mAutocompleteController;

    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mProfileSupplier = ObservableSuppliers.createMonotonic(mProfile);
        doReturn(new UserDataHost()).when(mLocationBarDataProvider).getUserDataHost();
        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxQueryControllerBridge);
        AutocompleteController.setInstanceForTesting(mAutocompleteController);
    }

    @Test
    public void testSetActiveTool() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        Runnable onFullyActivated =
                () -> {
                    AutocompleteInput input = session.getAutocompleteInput();
                    input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
                    verify(mComposeboxQueryControllerBridge)
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

                    clearInvocations(mComposeboxQueryControllerBridge);
                    input.setHasAttachments(true);
                    verify(mComposeboxQueryControllerBridge)
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
                };

        session.activate(mProfileSupplier, onFullyActivated);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testSetActiveTool_disabledShowModelPicker() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        Runnable onFullyActivated =
                () -> {
                    AutocompleteInput input = session.getAutocompleteInput();
                    input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
                    verify(mComposeboxQueryControllerBridge, never())
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
                };

        session.activate(mProfileSupplier, onFullyActivated);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testToolModeObserver() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        assertTrue(session.getAutocompleteInput().getToolModeSupplier().hasObservers());
        session.destroy();
        assertFalse(session.getAutocompleteInput().getToolModeSupplier().hasObservers());
    }

    @Test
    public void testToolModeObserver_disabledShowModelPicker() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        assertFalse(session.getAutocompleteInput().getToolModeSupplier().hasObservers());
    }
}
