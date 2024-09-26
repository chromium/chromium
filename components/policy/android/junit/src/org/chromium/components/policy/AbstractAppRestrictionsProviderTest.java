// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicInteger;

/** Robolectric test for AbstractAppRestrictionsProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class AbstractAppRestrictionsProviderTest {
    /** Minimal concrete class implementing AbstractAppRestrictionsProvider. */
    private static class DummyAppRestrictionsProvider extends AbstractAppRestrictionsProvider {
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

    private static class DummyContext extends ContextWrapper {
        public DummyContext(Context baseContext) {
            super(baseContext);
            mReceiverCount = new AtomicInteger(0);
            mLastRegisteredReceiverFlags = new AtomicInteger(0);
        }

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String broadcastPermission,
                Handler scheduler,
                int flags) {
            Intent intent =
                    super.registerReceiver(receiver, filter, broadcastPermission, scheduler, flags);
            mReceiverCount.getAndIncrement();
            mLastRegisteredReceiverFlags.set(flags);
            return intent;
        }

        @Override
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
            Intent intent = super.registerReceiver(receiver, filter);
            mReceiverCount.getAndIncrement();
            return intent;
        }

        @Override
        public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter, int flags) {
            Intent intent = super.registerReceiver(receiver, filter, flags);
            mReceiverCount.getAndIncrement();
            mLastRegisteredReceiverFlags.set(flags);
            return intent;
        }

        @Override
        public Intent registerReceiver(
                BroadcastReceiver receiver,
                IntentFilter filter,
                String broadcastPermission,
                Handler scheduler) {
            Intent intent =
                    super.registerReceiver(receiver, filter, broadcastPermission, scheduler);
            mReceiverCount.getAndIncrement();
            return intent;
        }

        @Override
        public void unregisterReceiver(BroadcastReceiver receiver) {
            // Not to unregisterReceiver in Android o+,  otherwise roboletric throws exception
            // because it doesn't override registerReceiver() with flag paramenter.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                super.unregisterReceiver(receiver);
            }
            mReceiverCount.getAndDecrement();
        }

        public int getReceiverCount() {
            return mReceiverCount.get();
        }

        public int getLastRegisteredReceiverFlags() {
            return mLastRegisteredReceiverFlags.get();
        }

        private AtomicInteger mReceiverCount;
        private AtomicInteger mLastRegisteredReceiverFlags;
    }

    /** Test method for {@link AbstractAppRestrictionsProvider#refresh()}. */
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

        // Set up the buffer to be returned by getApplicationRestrictions.
        when(provider.getApplicationRestrictions(anyString())).thenReturn(b1);

        // Prepare the provider
        CombinedPolicyProvider combinedProvider = mock(CombinedPolicyProvider.class);
        provider.setManagerAndSource(combinedProvider, 0);

        provider.refresh();
        verify(provider).getApplicationRestrictions(anyString());
        verify(combinedProvider).onSettingsAvailable(0, b1);
    }

    /** Test method for {@link AbstractAppRestrictionsProvider#startListeningForPolicyChanges()}. */
    @Test
    public void testStartListeningForPolicyChanges() {
        DummyContext dummyContext = new DummyContext(ApplicationProvider.getApplicationContext());
        AbstractAppRestrictionsProvider provider =
                spy(new DummyAppRestrictionsProvider(dummyContext));
        Intent intent = new Intent("org.chromium.test.policy.Hello");

        // If getRestrictionsChangeIntentAction returns null then we should not start a broadcast
        // receiver.
        provider.startListeningForPolicyChanges();
        Assert.assertEquals(0, dummyContext.getReceiverCount());

        // If it returns a string then we should.
        when(provider.getRestrictionChangeIntentAction())
                .thenReturn("org.chromium.test.policy.Hello");
        provider.startListeningForPolicyChanges();
        Assert.assertEquals(1, dummyContext.getReceiverCount());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Ensure that neither RECEIVER_EXPORTED nor RECEIVER_NOT_EXPORTED flags are set,
            // asserting that the receiver was only registered for protected broadcasts.
            final int badMask = ContextUtils.RECEIVER_EXPORTED | ContextUtils.RECEIVER_NOT_EXPORTED;
            Assert.assertEquals(0, dummyContext.getLastRegisteredReceiverFlags() & badMask);
        }
    }

    /** Test method for {@link AbstractAppRestrictionsProvider#stopListening()}. */
    @Test
    public void testStopListening() {
        DummyContext dummyContext = new DummyContext(ApplicationProvider.getApplicationContext());
        AbstractAppRestrictionsProvider provider =
                spy(new DummyAppRestrictionsProvider(dummyContext));
        Intent intent = new Intent("org.chromium.test.policy.Hello");

        // First try with null result from getRestrictionsChangeIntentAction, only test here is no
        // crash.
        provider.stopListening();

        // Now try starting and stopping listening properly.
        when(provider.getRestrictionChangeIntentAction())
                .thenReturn("org.chromium.test.policy.Hello");
        provider.startListeningForPolicyChanges();
        Assert.assertEquals(1, dummyContext.getReceiverCount());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Ensure that neither RECEIVER_EXPORTED nor RECEIVER_NOT_EXPORTED flags are set,
            // asserting that the receiver was only registered for protected broadcasts.
            final int badMask = ContextUtils.RECEIVER_EXPORTED | ContextUtils.RECEIVER_NOT_EXPORTED;
            Assert.assertEquals(0, dummyContext.getLastRegisteredReceiverFlags() & badMask);
        }
        provider.stopListening();
        Assert.assertEquals(0, dummyContext.getReceiverCount());
    }
}
