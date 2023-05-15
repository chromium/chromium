// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static android.os.Looper.getMainLooper;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for BroadcastReceiverScope.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class BroadcastReceiverScopeTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testConstructorRegistersReceiver() {
        String action = "org.chromium.chromecast.test.ACTION_HELLO";
        IntentFilter filter = new IntentFilter();
        filter.addAction(action);
        List<String> result = new ArrayList<>();
        new BroadcastReceiverScope(mContext, filter,
                (Intent intent) -> result.add("Intent received: " + intent.getAction()));
        Intent intent = new Intent().setAction(action);
        mContext.sendBroadcast(intent);
        shadowOf(getMainLooper()).idle();
        assertThat(result, contains("Intent received: org.chromium.chromecast.test.ACTION_HELLO"));
    }

    @Test
    public void testCallbackNotCalledIfBroadcastDoesNotMeetFilterSpec() {
        String helloAction = "org.chromium.chromecast.test.ACTION_HELLO";
        String goodbyeAction = "org.chromium.chromecast.test.ACTION_GOODBYE";
        IntentFilter filter = new IntentFilter();
        filter.addAction(helloAction);
        List<String> result = new ArrayList<>();
        new BroadcastReceiverScope(mContext, filter,
                (Intent intent) -> result.add("Intent received: " + intent.getAction()));
        Intent intent = new Intent().setAction(goodbyeAction);
        mContext.sendBroadcast(intent);
        shadowOf(getMainLooper()).idle();
        assertThat(result, emptyIterable());
    }

    @Test
    public void testCloseUnregistersReceiver() {
        String action = "org.chromium.chromecast.test.ACTION_HELLO";
        IntentFilter filter = new IntentFilter();
        filter.addAction(action);
        List<String> result = new ArrayList<>();
        // Wrap scope in try-with-resources to call close() on it.
        try (AutoCloseable scope = new BroadcastReceiverScope(mContext, filter,
                     (Intent intent) -> result.add("Intent received: " + intent.getAction()))) {
        } catch (Exception e) {
            result.add("Exception during lifetime of BroadcastReceiver scope: " + e);
        }
        Intent intent = new Intent().setAction(action);
        mContext.sendBroadcast(intent);
        shadowOf(getMainLooper()).idle();
        assertThat(result, emptyIterable());
    }
}
