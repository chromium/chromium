// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;

import android.content.Intent;
import android.content.IntentFilter;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for LocalBroadcastReceiverScope.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class LocalBroadcastReceiverScopeTest {
    @Test
    public void testConstructorRegistersReceiver() {
        String action = "org.chromium.chromecast.test.ACTION_HELLO";
        IntentFilter filter = new IntentFilter();
        filter.addAction(action);
        List<String> result = new ArrayList<>();
        LocalBroadcastManager broadcastManager =
                LocalBroadcastManager.getInstance(RuntimeEnvironment.application);
        new LocalBroadcastReceiverScope(broadcastManager, filter,
                (Intent intent) -> result.add("Intent received: " + intent.getAction()));
        Intent intent = new Intent().setAction(action);
        broadcastManager.sendBroadcast(intent);
        assertThat(result, contains("Intent received: org.chromium.chromecast.test.ACTION_HELLO"));
    }

    @Test
    public void testCallbackNotCalledIfBroadcastDoesNotMeetFilterSpec() {
        String helloAction = "org.chromium.chromecast.test.ACTION_HELLO";
        String goodbyeAction = "org.chromium.chromecast.test.ACTION_GOODBYE";
        IntentFilter filter = new IntentFilter();
        filter.addAction(helloAction);
        List<String> result = new ArrayList<>();
        LocalBroadcastManager broadcastManager =
                LocalBroadcastManager.getInstance(RuntimeEnvironment.application);
        new LocalBroadcastReceiverScope(broadcastManager, filter,
                (Intent intent) -> result.add("Intent received: " + intent.getAction()));
        Intent intent = new Intent().setAction(goodbyeAction);
        broadcastManager.sendBroadcast(intent);
        assertThat(result, emptyIterable());
    }

    @Test
    public void testCloseUnregistersReceiver() {
        String action = "org.chromium.chromecast.test.ACTION_HELLO";
        IntentFilter filter = new IntentFilter();
        filter.addAction(action);
        List<String> result = new ArrayList<>();
        LocalBroadcastManager broadcastManager =
                LocalBroadcastManager.getInstance(RuntimeEnvironment.application);
        // Wrap scope in try-with-resources to call close() on it.
        try (AutoCloseable scope = new LocalBroadcastReceiverScope(broadcastManager, filter,
                     (Intent intent) -> result.add("Intent received: " + intent.getAction()))) {
        } catch (Exception e) {
            result.add("Exception during lifetime of BroadcastReceiver scope: " + e);
        }
        Intent intent = new Intent().setAction(action);
        broadcastManager.sendBroadcast(intent);
        assertThat(result, emptyIterable());
    }
}
