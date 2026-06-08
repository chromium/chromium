// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.url.GURL;

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
        doReturn(new FuseboxSessionState()).when(mLocationBarDataProvider).getFuseboxSessionState();
        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxQueryControllerBridge);
        AutocompleteController.setInstanceForTesting(mAutocompleteController);
    }

    @Test
    public void testSetActiveTool() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        FuseboxSessionState session = new FuseboxSessionState();
        Runnable onFullyActivated =
                () -> {
                    AutocompleteInput input = session.getAutocompleteInput();
                    input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
                    verify(mComposeboxQueryControllerBridge)
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

                    clearInvocations(mComposeboxQueryControllerBridge);
                    input.setHasAttachments(true);
                    verify(mComposeboxQueryControllerBridge, never()).setActiveTool(anyInt());
                };

        session.activate(
                ContextUtils.getApplicationContext(), null, mProfileSupplier, onFullyActivated);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testSetActiveTool_disabledShowModelPicker() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        FuseboxSessionState session = new FuseboxSessionState();
        Runnable onFullyActivated =
                () -> {
                    AutocompleteInput input = session.getAutocompleteInput();
                    input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
                    verify(mComposeboxQueryControllerBridge, never())
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);
                };

        session.activate(
                ContextUtils.getApplicationContext(), null, mProfileSupplier, onFullyActivated);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testRequestTypeObserver() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        FuseboxSessionState session = new FuseboxSessionState();
        assertTrue(session.getAutocompleteInput().getRequestTypeSupplier().hasObservers());
        session.destroy();
        assertFalse(session.getAutocompleteInput().getRequestTypeSupplier().hasObservers());
    }

    @Test
    public void testRequestTypeObserver_disabledShowModelPicker() {
        OmniboxFeatures.sShowModelPicker.setForTesting(false);
        FuseboxSessionState session = new FuseboxSessionState();
        assertFalse(session.getAutocompleteInput().getRequestTypeSupplier().hasObservers());
    }

    @Test
    public void testFrom() {
        doReturn("Title").when(mLocationBarDataProvider).getTitle();
        doReturn(new GURL("https://www.google.com"))
                .when(mLocationBarDataProvider)
                .getCurrentGurl();
        doReturn(1).when(mLocationBarDataProvider).getPageClassification(false);

        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        assertNotNull(session);
        assertEquals("Title", session.getAutocompleteInput().getPageTitle());
        assertEquals(
                "https://www.google.com/", session.getAutocompleteInput().getPageUrl().getSpec());
        assertEquals(1, session.getAutocompleteInput().getPageClassification());
    }

    @Test
    public void testActivate() {
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        assertFalse(session.isSessionActive());
        assertNull(session.getProfile());

        Runnable onFullyActivated = mock(Runnable.class);
        session.activate(
                ContextUtils.getApplicationContext(), null, mProfileSupplier, onFullyActivated);

        assertTrue(session.isSessionActive());
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(mProfile, session.getProfile());
        assertEquals(mAutocompleteController, session.getAutocompleteController());
        assertEquals(
                mComposeboxQueryControllerBridge, session.getComposeboxQueryControllerBridge());
        assertNotNull(session.getFuseboxAttachmentModelList());
        verify(onFullyActivated).run();
        verify(mAutocompleteController)
                .setComposeboxQueryControllerBridge(mComposeboxQueryControllerBridge);

        // Simulate re-activation
        clearInvocations(mAutocompleteController);
        session.activate(
                ContextUtils.getApplicationContext(), null, mProfileSupplier, onFullyActivated);
        verify(mAutocompleteController)
                .setComposeboxQueryControllerBridge(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testActivate_nullComposebox() {
        ComposeboxQueryControllerBridge.setInstanceForTesting(null);
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        assertFalse(session.isSessionActive());
        assertNull(session.getProfile());

        Runnable onFullyActivated = mock(Runnable.class);
        session.activate(
                ContextUtils.getApplicationContext(), null, mProfileSupplier, onFullyActivated);

        assertTrue(session.isSessionActive());
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(mProfile, session.getProfile());
        assertEquals(mAutocompleteController, session.getAutocompleteController());
        assertNull(session.getComposeboxQueryControllerBridge());
        assertNull(session.getFuseboxAttachmentModelList());
        verify(onFullyActivated).run();

        verify(mAutocompleteController).setComposeboxQueryControllerBridge(null);

        // Simulate re-activation
        clearInvocations(mAutocompleteController);
        session.activate(
                ContextUtils.getApplicationContext(), null, mProfileSupplier, onFullyActivated);
        verify(mAutocompleteController).setComposeboxQueryControllerBridge(null);
    }

    @Test
    public void testDeactivate() {
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        session.activate(ContextUtils.getApplicationContext(), null, mProfileSupplier, null);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(session.isSessionActive());

        session.deactivate();
        assertFalse(session.isSessionActive());
        assertNull(session.getProfile());
        assertNull(session.getAutocompleteController());
        assertNull(session.getComposeboxQueryControllerBridge());
        assertNull(session.getFuseboxAttachmentModelList());
        verify(mAutocompleteController).setComposeboxQueryControllerBridge(null);
        verify(mComposeboxQueryControllerBridge).destroy();
    }

    @Test
    public void testDestroy() {
        OmniboxFeatures.sShowModelPicker.setForTesting(true);
        FuseboxSessionState session = new FuseboxSessionState();
        session.activate(ContextUtils.getApplicationContext(), null, mProfileSupplier, null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(session.getAutocompleteInput().getRequestTypeSupplier().hasObservers());
        session.destroy();
        assertFalse(session.getAutocompleteInput().getRequestTypeSupplier().hasObservers());
        assertNull(session.getProfile());
        assertNull(session.getAutocompleteController());
        assertNull(session.getComposeboxQueryControllerBridge());
        assertNull(session.getFuseboxAttachmentModelList());
    }

    @Test
    public void testActivate_defaultPageClassification_setsInitialUserText() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        UrlBarData.setShouldShowUrlForTesting(true);
        doReturn("Title").when(mLocationBarDataProvider).getTitle();
        doReturn(new GURL("https://www.google.com"))
                .when(mLocationBarDataProvider)
                .getCurrentGurl();
        doReturn(PageClassification.OTHER_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(false);

        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        session.activate(ContextUtils.getApplicationContext(), null, mProfileSupplier, null);

        assertEquals("www.google.com", session.getAutocompleteInput().getUserText());
    }

    @Test
    public void testActivate_searchWidgetPageClassification_doesNotSetInitialUserText() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        UrlBarData.setShouldShowUrlForTesting(true);
        doReturn("Title").when(mLocationBarDataProvider).getTitle();
        doReturn(new GURL("https://www.google.com"))
                .when(mLocationBarDataProvider)
                .getCurrentGurl();
        doReturn(PageClassification.ANDROID_SEARCH_WIDGET_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(false);

        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        session.activate(ContextUtils.getApplicationContext(), null, mProfileSupplier, null);

        assertEquals("", session.getAutocompleteInput().getUserText());
    }

    @Test
    public void testActivate_shortcutsWidgetPageClassification_doesNotSetInitialUserText() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        UrlBarData.setShouldShowUrlForTesting(true);
        doReturn("Title").when(mLocationBarDataProvider).getTitle();
        doReturn(new GURL("https://www.google.com"))
                .when(mLocationBarDataProvider)
                .getCurrentGurl();
        doReturn(PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(false);

        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        session.activate(ContextUtils.getApplicationContext(), null, mProfileSupplier, null);

        assertEquals("", session.getAutocompleteInput().getUserText());
    }
}
