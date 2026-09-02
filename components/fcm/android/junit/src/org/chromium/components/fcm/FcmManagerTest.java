// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.fcm;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import com.google.android.gms.tasks.Tasks;
import com.google.firebase.FirebaseApp;
import com.google.firebase.FirebaseOptions;
import com.google.firebase.installations.FirebaseInstallations;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link FcmManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FcmManagerTest {
    private static final String TEST_INSTALLATION_ID = "test-fid-abcdef";

    @Mock private FirebaseApp mMockFirebaseApp;
    @Mock private FirebaseInstallations mMockInstallations;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        FcmManager.setInstanceForTesting(null);
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
    }

    @Test(expected = AssertionError.class)
    public void testGetInstanceOnUiThreadAsserts() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(false);
        FcmManager.getInstance();
    }

    @Test
    public void testLazyInitialization() {
        assertFalse(FcmManager.isInitialized());
        assertNotNull(FcmManager.getInstance());
        assertTrue(FcmManager.isInitialized());
    }

    @Test
    public void testReusesExistingFirebaseApp() {
        Context context = ContextUtils.getApplicationContext();
        FirebaseOptions options =
                new FirebaseOptions.Builder()
                        .setProjectId("existing-project-id")
                        .setApplicationId("1:123456789012:android:0123456789abcdef012345")
                        .setApiKey("unused")
                        .build();
        FirebaseApp existingApp = FirebaseApp.initializeApp(context, options, "ChromeFcmApp");
        try {
            FcmManager manager = new FcmManager(context, "existing-project-id");
            assertEquals(existingApp, manager.getFirebaseApp());
        } finally {
            existingApp.delete();
        }
    }

    @Test
    public void testFetchInstallationId() {
        when(mMockInstallations.getId()).thenReturn(Tasks.forResult(TEST_INSTALLATION_ID));
        FcmManager manager = new FcmManager(mMockFirebaseApp, mMockInstallations);

        String[] resultHolder = new String[1];
        manager.fetchInstallationId(id -> resultHolder[0] = id);
        ShadowLooper.idleMainLooper();

        assertEquals(TEST_INSTALLATION_ID, resultHolder[0]);
        verify(mMockInstallations).getId();
    }

    @Test
    public void testFetchInstallationIdFailure() {
        when(mMockInstallations.getId()).thenReturn(Tasks.forException(new Exception("error")));
        FcmManager manager = new FcmManager(mMockFirebaseApp, mMockInstallations);

        String[] resultHolder = new String[1];
        manager.fetchInstallationId(id -> resultHolder[0] = id);
        ShadowLooper.idleMainLooper();

        assertEquals("", resultHolder[0]);
        verify(mMockInstallations).getId();
    }

    @Test
    public void testDeleteInstallationId() {
        when(mMockInstallations.delete()).thenReturn(Tasks.forResult(null));
        FcmManager manager = new FcmManager(mMockFirebaseApp, mMockInstallations);

        boolean[] resultHolder = new boolean[1];
        manager.deleteInstallationId(success -> resultHolder[0] = success);
        ShadowLooper.idleMainLooper();

        assertTrue(resultHolder[0]);
        verify(mMockInstallations).delete();
    }
}
