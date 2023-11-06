// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link ChromeTransitionDrawable} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeTransitionDrawableTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private Drawable mInitialDrawable;
    @Mock private Drawable mFinalDrawable;

    private ChromeTransitionDrawable mTransitionDrawable;
    private int mInitialDrawableAlpha;
    private int mFinalDrawableAlpha;

    @Before
    public void setUp() {
        doReturn(mInitialDrawable).when(mInitialDrawable).mutate();
        doReturn(mFinalDrawable).when(mFinalDrawable).mutate();
        doAnswer(
                        invocation -> {
                            mInitialDrawableAlpha = invocation.getArgument(0);
                            return null;
                        })
                .when(mInitialDrawable)
                .setAlpha(anyInt());
        doAnswer(
                        invocation -> {
                            mFinalDrawableAlpha = invocation.getArgument(0);
                            return null;
                        })
                .when(mFinalDrawable)
                .setAlpha(anyInt());
        mInitialDrawableAlpha = 255;
        mFinalDrawableAlpha = 255;
        mTransitionDrawable = new ChromeTransitionDrawable(mInitialDrawable, mFinalDrawable);
        mTransitionDrawable.setCrossFadeEnabled(true);
    }

    @Test
    public void testTransition() {
        AtomicBoolean endActionRan = new AtomicBoolean(false);
        mTransitionDrawable
                .startTransition()
                .setDuration(100)
                .withEndAction(() -> endActionRan.set(true));
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255 - mFinalDrawableAlpha, mInitialDrawableAlpha);

        while (mFinalDrawableAlpha < 255) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(0, mInitialDrawableAlpha);
        assertTrue(endActionRan.get());
    }

    @Test
    public void testTransitionInterrupted() {
        mTransitionDrawable.startTransition();
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255 - mFinalDrawableAlpha, mInitialDrawableAlpha);

        mTransitionDrawable.startTransition();

        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        ShadowLooper.idleMainLooper();
        assertEquals(255, mFinalDrawableAlpha);
        assertEquals(0, mInitialDrawableAlpha);
    }

    @Test
    public void testTransitionNoCrossfade() {
        mTransitionDrawable.setCrossFadeEnabled(false);
        AtomicBoolean endActionRan = new AtomicBoolean(false);
        mTransitionDrawable
                .startTransition()
                .setDuration(100)
                .withEndAction(() -> endActionRan.set(true));
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255, mInitialDrawableAlpha);

        while (mFinalDrawableAlpha < 255) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255, mInitialDrawableAlpha);
        assertTrue(endActionRan.get());
    }

    @Test
    public void testFinishTransition_toInitial() {
        mTransitionDrawable.startTransition();
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255 - mFinalDrawableAlpha, mInitialDrawableAlpha);
        mTransitionDrawable.finishTransition(false);

        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);
    }

    @Test
    public void testFinishTransition_toFinal() {
        mTransitionDrawable.startTransition();
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255 - mFinalDrawableAlpha, mInitialDrawableAlpha);
        mTransitionDrawable.finishTransition(true);

        assertEquals(255, mFinalDrawableAlpha);
        assertEquals(0, mInitialDrawableAlpha);
    }

    @Test
    public void testReverseTransition() {
        AtomicBoolean endActionRan = new AtomicBoolean(false);
        mTransitionDrawable
                .startTransition()
                .setDuration(100)
                .withEndAction(() -> endActionRan.set(true));
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        assertEquals(255 - mFinalDrawableAlpha, mInitialDrawableAlpha);

        mTransitionDrawable.reverseTransition();
        assertEquals(255 - mFinalDrawableAlpha, mInitialDrawableAlpha);
        assertFalse(endActionRan.get());

        ShadowLooper.idleMainLooper();
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);
    }

    @Test
    public void testEndAction() {
        AtomicBoolean endActionRan = new AtomicBoolean(false);
        AtomicBoolean endActionRan2 = new AtomicBoolean(false);

        mTransitionDrawable
                .startTransition()
                .setDuration(100)
                .withEndAction(() -> endActionRan.set(true));
        ShadowLooper.runMainLooperOneTask();

        mTransitionDrawable.reverseTransition().withEndAction(() -> endActionRan2.set(true));
        ShadowLooper.idleMainLooper();
        assertFalse(endActionRan.get());
        assertTrue(endActionRan2.get());
    }

    @Test
    public void testSetCrossfadeEnabled() {
        mTransitionDrawable.setCrossFadeEnabled(true);
        mTransitionDrawable.startTransition().setDuration(100);
        assertEquals(255, mInitialDrawableAlpha);
        assertEquals(0, mFinalDrawableAlpha);

        while (mFinalDrawableAlpha < 120) {
            ShadowLooper.runMainLooperOneTask();
        }

        int initialDrawableAlphaBeforeSet = mInitialDrawableAlpha;
        assertEquals(255 - mFinalDrawableAlpha, initialDrawableAlphaBeforeSet);

        mTransitionDrawable.setCrossFadeEnabled(false);
        assertEquals(255, mInitialDrawableAlpha);

        mTransitionDrawable.setCrossFadeEnabled(true);
        assertEquals(initialDrawableAlphaBeforeSet, mInitialDrawableAlpha);
    }
}
