// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link BroadcastReceiver} that listens for audio becoming noisy (headset unplug).
 *
 * <p>This class is a singleton that is never destroyed as it is an application level broadcast
 * receiver. Care should be taken to avoid memory leaks by removing listeners when they are no
 * longer needed.
 */
@NullMarked
public class AudioBecomingNoisyReceiver extends BroadcastReceiver {
    private static final String TAG = "AudioNoisyReceiver";

    /** A listener for audio becoming noisy broadcasts. */
    public interface AudioBecomingNoisyObserver {
        void onAudioBecomingNoisy();
    }

    private static @Nullable AudioBecomingNoisyReceiver sInstance;

    private final ObserverList<AudioBecomingNoisyObserver> mObservers = new ObserverList<>();
    private boolean mIsRegistered;

    private AudioBecomingNoisyReceiver() {}

    /**
     * Adds an observer for audio becoming noisy broadcasts.
     *
     * @param observer The observer to add.
     */
    public static void addObserver(AudioBecomingNoisyObserver observer) {
        ThreadUtils.assertOnUiThread();
        AudioBecomingNoisyReceiver instance = getInstance();
        if (instance.mObservers.isEmpty()) {
            instance.register();
        }
        instance.mObservers.addObserver(observer);
    }

    /**
     * Removes an observer for audio becoming noisy broadcasts.
     *
     * @param observer The observer to remove.
     */
    public static void removeObserver(AudioBecomingNoisyObserver observer) {
        ThreadUtils.assertOnUiThread();
        AudioBecomingNoisyReceiver instance = getInstance();
        instance.mObservers.removeObserver(observer);
        if (instance.mObservers.isEmpty()) {
            instance.unregister();
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        ThreadUtils.assertOnUiThread();
        if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction())) {
            for (AudioBecomingNoisyObserver observer : mObservers) {
                observer.onAudioBecomingNoisy();
            }
        }
    }

    /** Returns the singleton instance of the {@link AudioBecomingNoisyReceiver}. */
    @VisibleForTesting
    public static AudioBecomingNoisyReceiver getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new AudioBecomingNoisyReceiver();
            // Register a resetter to prevent this singleton from leaking the Android context
            // and registered observers between JUnit tests in the same JVM.
            ResettersForTesting.register(AudioBecomingNoisyReceiver::resetForTesting);
        }
        return sInstance;
    }

    private void register() {
        ThreadUtils.assertOnUiThread();
        if (mIsRegistered) return;
        mIsRegistered = true;
        IntentFilter filter = new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), AudioBecomingNoisyReceiver.this, filter);
    }

    private void unregister() {
        ThreadUtils.assertOnUiThread();
        if (!mIsRegistered) return;
        mIsRegistered = false;
        try {
            ContextUtils.getApplicationContext()
                    .unregisterReceiver(AudioBecomingNoisyReceiver.this);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Failed to unregister receiver, it might not be registered", e);
        }
    }

    public boolean isRegisteredForTesting() {
        return mIsRegistered;
    }

    public static void resetForTesting() {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) {
            sInstance.unregister();
            sInstance = null;
        }
    }
}
