// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.media.AudioAttributes;
import android.media.AudioManager;
import android.os.Build;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAudioManager;

import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.ReactiveRecorder;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for CastAudioManager.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastAudioManagerTest {
    // An example request that can be provided to requestAudioFocus().
    private static CastAudioFocusRequest buildFocusRequest() {
        return new CastAudioFocusRequest.Builder()
                .setFocusGain(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(new AudioAttributes.Builder()
                                            .setUsage(AudioAttributes.USAGE_MEDIA)
                                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                                            .build())
                .build();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testAudioFocusScopeDeactivatesWhenRequestGranted() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        requestAudioFocusState.set(buildFocusRequest());
        lostAudioFocusRecorder.verify().closed(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testAudioFocusLostWhenFocusRequestStateIsReset() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        requestAudioFocusState.set(buildFocusRequest());
        shadowAudioManager.getLastAudioFocusRequest().listener.onAudioFocusChange(
                AudioManager.AUDIOFOCUS_GAIN);
        lostAudioFocusRecorder.verify().closed(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        requestAudioFocusState.reset();
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NORMAL).end();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testAudioFocusScopeActivatedWhenAudioFocusIsLostButRequestStillActive() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        requestAudioFocusState.set(buildFocusRequest());
        AudioManager.OnAudioFocusChangeListener listener =
                shadowAudioManager.getLastAudioFocusRequest().listener;
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        lostAudioFocusRecorder.verify().closed(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NORMAL).end();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testAudioFocusScopeWhenAudioFocusIsLostAndRegained() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        requestAudioFocusState.set(buildFocusRequest());
        AudioManager.OnAudioFocusChangeListener listener =
                shadowAudioManager.getLastAudioFocusRequest().listener;
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        lostAudioFocusRecorder.verify().closed(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NORMAL).end();
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        lostAudioFocusRecorder.verify().closed(CastAudioManager.AudioFocusLoss.NORMAL).end();
    }

    @Test
    public void testAudioFocusNotGainedIfRequestNotActivated() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.NOT_REQUESTED).end();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testNoAudioFocusLossIfRequestGrantedImmediately() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        requestAudioFocusState.set(buildFocusRequest());
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        lostAudioFocusRecorder.verify().end();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testTransientAudioFocusLoss() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        requestAudioFocusState.set(buildFocusRequest());
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        AudioManager.OnAudioFocusChangeListener listener =
                shadowAudioManager.getLastAudioFocusRequest().listener;
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
        lostAudioFocusRecorder.verify().opened(CastAudioManager.AudioFocusLoss.TRANSIENT).end();
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        lostAudioFocusRecorder.verify().closed(CastAudioManager.AudioFocusLoss.TRANSIENT).end();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testTransientCanDuckAudioFocusLoss() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<CastAudioFocusRequest> requestAudioFocusState = new Controller<>();
        requestAudioFocusState.set(buildFocusRequest());
        Observable<CastAudioManager.AudioFocusLoss> lostAudioFocusState =
                audioManager.requestAudioFocusWhen(requestAudioFocusState);
        ReactiveRecorder lostAudioFocusRecorder = ReactiveRecorder.record(lostAudioFocusState);
        AudioManager.OnAudioFocusChangeListener listener =
                shadowAudioManager.getLastAudioFocusRequest().listener;
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        lostAudioFocusRecorder.verify()
                .opened(CastAudioManager.AudioFocusLoss.TRANSIENT_CAN_DUCK)
                .end();
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        lostAudioFocusRecorder.verify()
                .closed(CastAudioManager.AudioFocusLoss.TRANSIENT_CAN_DUCK)
                .end();
    }
}
