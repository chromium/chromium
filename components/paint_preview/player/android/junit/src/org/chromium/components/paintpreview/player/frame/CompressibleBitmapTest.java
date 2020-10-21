// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the {@link CompressibleBitmap} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CompressibleBitmapTest.FakeShadowBitmapFactory.class})
public class CompressibleBitmapTest {
    /**
     * A fake {@link BitmapFactory} used to avoid native for decoding.
     */
    @Implements(BitmapFactory.class)
    public static class FakeShadowBitmapFactory {
        private static Bitmap sBitmap;

        public static void setBitmap(Bitmap bitmap) {
            sBitmap = bitmap;
        }

        @Implementation
        public static Bitmap decodeByteArray(byte[] array, int offset, int length) {
            return sBitmap;
        }
    }

    @Test
    public void testCompressAndDiscard() {
        Bitmap bitmap = Mockito.mock(Bitmap.class);
        when(bitmap.compress(any(), anyInt(), any())).thenReturn(true);

        SequencedTaskRunner taskRunner = Mockito.mock(SequencedTaskRunner.class);
        doAnswer(invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(taskRunner)
                .postTask(any());

        CompressibleBitmap compressibleBitmap = new CompressibleBitmap(bitmap, taskRunner, false);
        verify(bitmap, times(1)).compress(any(), eq(100), any());

        Assert.assertNull(compressibleBitmap.getBitmap());
    }

    @Test
    public void testCompressAndKeep() {
        Bitmap bitmap = Mockito.mock(Bitmap.class);
        when(bitmap.compress(any(), anyInt(), any())).thenReturn(true);

        SequencedTaskRunner taskRunner = Mockito.mock(SequencedTaskRunner.class);
        doAnswer(invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(taskRunner)
                .postTask(any());

        CompressibleBitmap compressibleBitmap = new CompressibleBitmap(bitmap, taskRunner, true);
        verify(bitmap, times(1)).compress(any(), eq(100), any());

        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);
        compressibleBitmap.discardBitmap();
        Assert.assertNull(compressibleBitmap.getBitmap());

        // Ensure doing this again doesn't crash.
        compressibleBitmap.discardBitmap();
    }

    @Test
    public void testNoDiscardIfCompressFails() {
        Bitmap bitmap = Mockito.mock(Bitmap.class);
        when(bitmap.compress(any(), anyInt(), any())).thenReturn(false);

        SequencedTaskRunner taskRunner = Mockito.mock(SequencedTaskRunner.class);
        doAnswer(invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(taskRunner)
                .postTask(any());

        CompressibleBitmap compressibleBitmap = new CompressibleBitmap(bitmap, taskRunner, false);
        verify(bitmap, times(1)).compress(any(), eq(100), any());

        // Discarding should fail.
        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);
        compressibleBitmap.discardBitmap();
        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);
    }

    @Test
    public void testInflate() throws TimeoutException {
        Bitmap bitmap = Mockito.mock(Bitmap.class);
        when(bitmap.compress(any(), anyInt(), any())).thenReturn(true);

        SequencedTaskRunner taskRunner = Mockito.mock(SequencedTaskRunner.class);
        doAnswer(invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(taskRunner)
                .postTask(any());

        CompressibleBitmap compressibleBitmap = new CompressibleBitmap(bitmap, taskRunner, false);
        verify(bitmap, times(1)).compress(any(), eq(100), any());
        Assert.assertNull(compressibleBitmap.getBitmap());

        FakeShadowBitmapFactory.setBitmap(bitmap);

        CallbackHelper helper = new CallbackHelper();
        compressibleBitmap.inflateInBackground(compressible -> { helper.notifyCalled(); });
        helper.waitForFirst();

        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);
        compressibleBitmap.destroy();
        Assert.assertNull(compressibleBitmap.getBitmap());

        // Inflation should fail if the CompressibleBitmap is destroyed.
        CallbackHelper inflatedNoBitmap = new CallbackHelper();
        compressibleBitmap.inflateInBackground(compressible -> {
            Assert.assertNull(compressible.getBitmap());
            inflatedNoBitmap.notifyCalled();
        });
        inflatedNoBitmap.waitForFirst();
    }

    @Test
    public void testLocking() throws TimeoutException {
        Bitmap bitmap = Mockito.mock(Bitmap.class);
        when(bitmap.compress(any(), anyInt(), any())).thenReturn(true);
        SequencedTaskRunner taskRunner = Mockito.mock(SequencedTaskRunner.class);
        doAnswer(invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(taskRunner)
                .postTask(any());

        CompressibleBitmap compressibleBitmap = new CompressibleBitmap(bitmap, taskRunner, true);
        verify(bitmap, times(1)).compress(any(), eq(100), any());
        Assert.assertTrue(compressibleBitmap.lock());
        Assert.assertFalse(compressibleBitmap.lock());
        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);

        compressibleBitmap.discardBitmap();
        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);

        compressibleBitmap.destroy();
        Assert.assertEquals(compressibleBitmap.getBitmap(), bitmap);
        Assert.assertTrue(compressibleBitmap.unlock());
        Assert.assertFalse(compressibleBitmap.unlock());

        compressibleBitmap.discardBitmap();
        Assert.assertTrue(compressibleBitmap.lock());
        Assert.assertNull(compressibleBitmap.getBitmap());
        Assert.assertTrue(compressibleBitmap.unlock());

        CallbackHelper helper = new CallbackHelper();
        compressibleBitmap.inflateInBackground(compressible -> { helper.notifyCalled(); });
        helper.waitForFirst();

        compressibleBitmap.destroy();
        Assert.assertTrue(compressibleBitmap.lock());
        Assert.assertNull(compressibleBitmap.getBitmap());
        Assert.assertTrue(compressibleBitmap.unlock());
        verify(taskRunner, times(2)).postDelayedTask(any(), anyLong());
    }
}
