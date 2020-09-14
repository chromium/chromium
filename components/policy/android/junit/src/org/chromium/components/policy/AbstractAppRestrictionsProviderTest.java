// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Robolectric test for AbstractAppRestrictionsProvider.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AbstractAppRestrictionsProviderTest {
    /**
     * Minimal concrete class implementing AbstractAppRestrictionsProvider.
     */
    private class DummyAppRestrictionsProvider extends AbstractAppRestrictionsProvider {
        public DummyAppRestrictionsProvider(Context context) {
            super(context);
        }

        @Override
        protected Bundle getApplicationRestrictions(String packageName) {
            return null;
        }

        @Override
        protected String getRestrictionChangeIntentAction() {
            return null;
        }
    }

    /**
     * Test method for {@link AbstractAppRestrictionsProvider#refresh()}.
     */
    @Test
    public void testRefresh() {
        // We want to control precisely when background tasks run
        Robolectric.getBackgroundThreadScheduler().pause();

        Context context = RuntimeEnvironment.application;

        // Clear the preferences
        ContextUtils.getAppSharedPreferences().edit().clear();

        // Set up a bundle for testing.
        Bundle b1 = new Bundle();
        b1.putString("Key1", "value1");
        b1.putInt("Key2", 42);

        // Mock out the histogram functions, since they call statics.
        AbstractAppRestrictionsProvider provider = spy(new DummyAppRestrictionsProvider(context));
        doNothing().when(provider).recordStartTimeHistogram(anyLong());

        // Set up the buffer to be returned by getApplicationRestrictions.
        when(provider.getApplicationRestrictions(anyString())).thenReturn(b1);

        // Prepare the provider
        CombinedPolicyProvider combinedProvider = mock(CombinedPolicyProvider.class);
        provider.setManagerAndSource(combinedProvider, 0);

        provider.refresh();
        verify(provider).getApplicationRestrictions(anyString());
        verify(provider).recordStartTimeHistogram(anyLong());
        verify(combinedProvider).onSettingsAvailable(0, b1);
    }

    /**
     * Test method for {@link AbstractAppRestrictionsProvider#startListeningForPolicyChanges()}.
     */
    @Test
    public void testStartListeningForPolicyChanges() {
        Context context = RuntimeEnvironment.application;
        AbstractAppRestrictionsProvider provider = spy(new DummyAppRestrictionsProvider(context));
        Intent intent = new Intent("org.chromium.test.policy.Hello");
        ShadowApplication shadowApplication = ShadowApplication.getInstance();

        // If getRestrictionsChangeIntentAction returns null then we should not start a broadcast
        // receiver.
        provider.startListeningForPolicyChanges();
        Assert.assertFalse(shadowApplication.hasReceiverForIntent(intent));

        // If it returns a string then we should.
        when(provider.getRestrictionChangeIntentAction())
                .thenReturn("org.chromium.test.policy.Hello");
        provider.startListeningForPolicyChanges();
        Assert.assertTrue(shadowApplication.hasReceiverForIntent(intent));
    }

    /**
     * Test method for {@link AbstractAppRestrictionsProvider#stopListening()}.
     */
    @Test
    public void testStopListening() {
        Context context = RuntimeEnvironment.application;
        AbstractAppRestrictionsProvider provider = spy(new DummyAppRestrictionsProvider(context));
        Intent intent = new Intent("org.chromium.test.policy.Hello");
        ShadowApplication shadowApplication = ShadowApplication.getInstance();

        // First try with null result from getRestrictionsChangeIntentAction, only test here is no
        // crash.
        provider.stopListening();

        // Now try starting and stopping listening properly.
        when(provider.getRestrictionChangeIntentAction())
                .thenReturn("org.chromium.test.policy.Hello");
        provider.startListeningForPolicyChanges();
        provider.stopListening();
        Assert.assertFalse(shadowApplication.hasReceiverForIntent(intent));
    }
}
