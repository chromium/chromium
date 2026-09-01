// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunService;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunServiceJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.Acceptability;
import org.chromium.components.autofill.AtMemoryPayload;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Collections;

@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryBottomSheetBridgeTest {
    private static final long NATIVE_BRIDGE = 100L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryBottomSheetBridge.Natives mNativeMock;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private PersonalContextFirstRunService.Natives mFirstRunServiceJniMock;

    private AtMemoryBottomSheetBridge mBridge;

    @Before
    public void setUp() throws Exception {
        AtMemoryBottomSheetBridgeJni.setInstanceForTesting(mNativeMock);
        PersonalContextFirstRunServiceJni.setInstanceForTesting(mFirstRunServiceJniMock);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(context));
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        mBridge = AtMemoryBottomSheetBridge.create(NATIVE_BRIDGE, mWindowAndroid, mProfile);
        assertNotNull(mBridge);
    }

    @Test
    @SmallTest
    public void testOnDismissedNotCalledAfterDestroy() {
        mBridge.destroy();

        mBridge.onDismissed();

        verify(mNativeMock, never()).onDismissed(NATIVE_BRIDGE);
    }

    @Test
    @SmallTest
    public void testCreateAutofillSuggestionWithPayload() {
        AtMemoryPayload payload = new AtMemoryPayload("Passport");
        AutofillSuggestion suggestion =
                AtMemoryBottomSheetBridge.createAutofillSuggestion(
                        "12345",
                        "Passport Number",
                        /* iconId= */ 0,
                        SuggestionType.AT_MEMORY_SEARCH_RESULT,
                        Collections.emptyList(),
                        Acceptability.SELECTABLE_AND_ACCEPTABLE,
                        /* hasDeactivatedStyle= */ false,
                        /* isLoading= */ false,
                        payload);

        assertEquals("12345", suggestion.getLabel());
        assertEquals("Passport Number", suggestion.getSublabel());
        assertEquals(payload, suggestion.getAtMemoryPayload());
        assertNotNull(suggestion.getAtMemoryPayload());
        assertEquals("Passport", suggestion.getAtMemoryPayload().getTypeName());
    }
}
