// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.os.SystemClock;

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
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachment;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;

/** Unit tests for {@link FuseboxSessionState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxSessionStateUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock Profile mProfile;
    private @Mock ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;

    private UserDataHost mUserDataHost;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mUserDataHost = new UserDataHost();
        mProfileSupplier = ObservableSuppliers.createMonotonic(mProfile);
        doReturn(mUserDataHost).when(mLocationBarDataProvider).getUserDataHost();
        ComposeboxQueryControllerBridge.setInstanceForTesting(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testPropagatesHasAttachments() {
        FuseboxSessionState session = FuseboxSessionState.from(mLocationBarDataProvider);
        Runnable onFullyActivated =
                () -> {
                    AutocompleteInput input = session.getAutocompleteInput();
                    input.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
                    verify(mComposeboxQueryControllerBridge)
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_VALUE);

                    session.getFuseboxAttachmentModelList()
                            .add(
                                    FuseboxAttachment.forFile(
                                            /* thumbnail= */ null,
                                            "title",
                                            "mimeType",
                                            "data".getBytes(),
                                            SystemClock.elapsedRealtime(),
                                            FuseboxAttachmentButtonType.FILES));
                    verify(mComposeboxQueryControllerBridge)
                            .setActiveTool(ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE);
                };
        session.activate(mProfileSupplier, onFullyActivated);
    }
}
