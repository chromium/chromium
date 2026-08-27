// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowSurfaceView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityView;

import java.util.Set;

/** Unit tests for {@link CompositorViewImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CompositorViewImplUnitTest {
    private static final long NATIVE_PTR = 12345L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CompositorViewImpl.Natives mMockJni;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private XrSceneCoreSessionManager mXrSceneCoreSessionManager;
    @Mock private XrSurfaceEntityHolder mXrSurfaceEntityHolder;
    @Captor private ArgumentCaptor<XrSurfaceEntityHolder.Callback> mXrCallbackCaptor;

    private Context mContext;
    private ThinWebViewConstraints mConstraints;

    @Before
    public void setUp() {
        CompositorViewImplJni.setInstanceForTesting(mMockJni);
        when(mMockJni.init(any(), any(), anyInt())).thenReturn(NATIVE_PTR);
        when(mXrSceneCoreSessionManager.createSurfaceEntity(anyInt()))
                .thenReturn(mXrSurfaceEntityHolder);
        mContext = ApplicationProvider.getApplicationContext();
        mConstraints = new ThinWebViewConstraints();
    }

    @Test
    public void testTextureView_creationAndCallbacks() {
        mConstraints.supportsOpacity = true;
        mConstraints.backgroundColor = Color.GREEN;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);
        verify(mMockJni).init(compositorView, mWindowAndroid, Color.GREEN);

        View view = compositorView.getView();
        assertTrue(view instanceof TextureView);
        TextureView textureView = (TextureView) view;

        TextureView.SurfaceTextureListener listener = textureView.getSurfaceTextureListener();
        assertNotNull(listener);

        SurfaceTexture surfaceTexture = new SurfaceTexture(0);
        textureView.setSurfaceTexture(surfaceTexture);

        // Surface texture available
        listener.onSurfaceTextureAvailable(surfaceTexture, 100, 200);
        verify(mMockJni).surfaceCreated(NATIVE_PTR);
        verify(mMockJni)
                .surfaceChanged(
                        eq(NATIVE_PTR),
                        eq(PixelFormat.OPAQUE),
                        eq(100),
                        eq(200),
                        eq(false),
                        any(Surface.class));

        // Size changed
        listener.onSurfaceTextureSizeChanged(surfaceTexture, 300, 400);
        verify(mMockJni)
                .surfaceChanged(
                        eq(NATIVE_PTR),
                        eq(PixelFormat.OPAQUE),
                        eq(300),
                        eq(400),
                        eq(false),
                        any(Surface.class));

        // Updated (should not invoke extra JNI methods)
        listener.onSurfaceTextureUpdated(surfaceTexture);

        // Surface texture destroyed
        boolean result = listener.onSurfaceTextureDestroyed(surfaceTexture);
        assertFalse(result);
        verify(mMockJni).surfaceDestroyed(NATIVE_PTR);

        // Surface texture available again
        listener.onSurfaceTextureAvailable(surfaceTexture, 300, 400);
        verify(mMockJni, times(2)).surfaceCreated(NATIVE_PTR);
    }

    @Test
    public void testTextureView_callbacksIgnoredAfterDestroy() {
        mConstraints.supportsOpacity = true;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);
        TextureView textureView = (TextureView) compositorView.getView();
        TextureView.SurfaceTextureListener listener = textureView.getSurfaceTextureListener();
        assertNotNull(listener);

        SurfaceTexture surfaceTexture = new SurfaceTexture(0);
        textureView.setSurfaceTexture(surfaceTexture);

        compositorView.destroy();
        verify(mMockJni).destroy(NATIVE_PTR);

        // All subsequent callbacks should no-op
        listener.onSurfaceTextureAvailable(surfaceTexture, 100, 200);
        listener.onSurfaceTextureSizeChanged(surfaceTexture, 100, 200);
        listener.onSurfaceTextureDestroyed(surfaceTexture);

        verify(mMockJni, never()).surfaceCreated(anyLong());
        verify(mMockJni, never()).surfaceDestroyed(anyLong());
        verify(mMockJni, never())
                .surfaceChanged(anyLong(), anyInt(), anyInt(), anyInt(), anyBoolean(), any());
    }

    @Test
    public void testSurfaceView_creationAndCallbacks() {
        mConstraints.supportsOpacity = false;
        mConstraints.backgroundColor = Color.BLUE;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(true);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);
        verify(mMockJni).init(compositorView, mWindowAndroid, Color.BLUE);

        View view = compositorView.getView();
        assertTrue(view instanceof SurfaceView);
        SurfaceView surfaceView = (SurfaceView) view;

        ShadowSurfaceView shadowSurfaceView = Shadows.shadowOf(surfaceView);
        Set<SurfaceHolder.Callback> callbacks =
                shadowSurfaceView.getFakeSurfaceHolder().getCallbacks();
        assertEquals(1, callbacks.size());
        SurfaceHolder.Callback callback = callbacks.iterator().next();

        SurfaceHolder holder = surfaceView.getHolder();
        Surface surface = holder.getSurface();

        callback.surfaceCreated(holder);
        verify(mMockJni).surfaceCreated(NATIVE_PTR);

        callback.surfaceChanged(holder, PixelFormat.RGBA_8888, 640, 480);
        verify(mMockJni)
                .surfaceChanged(
                        eq(NATIVE_PTR),
                        eq(PixelFormat.RGBA_8888),
                        eq(640),
                        eq(480),
                        eq(true),
                        eq(surface));

        callback.surfaceDestroyed(holder);
        verify(mMockJni).surfaceDestroyed(NATIVE_PTR);
    }

    @Test
    public void testSurfaceView_callbacksIgnoredAfterDestroy() {
        mConstraints.supportsOpacity = false;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(true);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);
        SurfaceView surfaceView = (SurfaceView) compositorView.getView();
        ShadowSurfaceView shadowSurfaceView = Shadows.shadowOf(surfaceView);
        SurfaceHolder.Callback callback =
                shadowSurfaceView.getFakeSurfaceHolder().getCallbacks().iterator().next();
        SurfaceHolder holder = surfaceView.getHolder();

        compositorView.destroy();
        verify(mMockJni).destroy(NATIVE_PTR);

        callback.surfaceCreated(holder);
        callback.surfaceChanged(holder, PixelFormat.RGBA_8888, 640, 480);
        callback.surfaceDestroyed(holder);

        verify(mMockJni, never()).surfaceCreated(anyLong());
        verify(mMockJni, never()).surfaceDestroyed(anyLong());
        verify(mMockJni, never())
                .surfaceChanged(anyLong(), anyInt(), anyInt(), anyInt(), anyBoolean(), any());
    }

    @Test
    public void testSpatialSurfaceView_creationAndCallbacks() {
        mConstraints.supportsOpacity = true;
        mConstraints.backgroundColor = Color.CYAN;

        CompositorViewImpl compositorView =
                new CompositorViewImpl(
                        mContext,
                        mWindowAndroid,
                        mConstraints,
                        mXrSceneCoreSessionManager,
                        XrSurfaceEntityShape.QUAD);
        verify(mMockJni).init(compositorView, mWindowAndroid, Color.CYAN);
        verify(mXrSceneCoreSessionManager).createSurfaceEntity(XrSurfaceEntityShape.QUAD);
        verify(mXrSurfaceEntityHolder).addCallback(mXrCallbackCaptor.capture());

        View view = compositorView.getView();
        assertTrue(view instanceof XrSurfaceEntityView);

        XrSurfaceEntityHolder.Callback callback = mXrCallbackCaptor.getValue();
        Surface mockSurface = mock(Surface.class);

        callback.surfaceCreated(mockSurface);
        verify(mMockJni).surfaceCreated(NATIVE_PTR);

        callback.surfaceChanged(mockSurface, 800, 600);
        verify(mMockJni)
                .surfaceChanged(
                        eq(NATIVE_PTR),
                        eq(PixelFormat.OPAQUE),
                        eq(800),
                        eq(600),
                        eq(false),
                        eq(mockSurface));

        callback.surfaceDestroyed();
        verify(mMockJni).surfaceDestroyed(NATIVE_PTR);

        compositorView.destroy();
        verify(mMockJni).destroy(NATIVE_PTR);
        verify(mXrSurfaceEntityHolder).dispose();
    }

    @Test
    public void testSpatialSurfaceView_callbacksIgnoredAfterDestroy() {
        mConstraints.supportsOpacity = true;

        CompositorViewImpl compositorView =
                new CompositorViewImpl(
                        mContext,
                        mWindowAndroid,
                        mConstraints,
                        mXrSceneCoreSessionManager,
                        XrSurfaceEntityShape.QUAD);
        verify(mXrSurfaceEntityHolder).addCallback(mXrCallbackCaptor.capture());
        XrSurfaceEntityHolder.Callback callback = mXrCallbackCaptor.getValue();
        Surface mockSurface = mock(Surface.class);

        compositorView.destroy();
        verify(mMockJni).destroy(NATIVE_PTR);

        callback.surfaceCreated(mockSurface);
        callback.surfaceChanged(mockSurface, 800, 600);
        callback.surfaceDestroyed();

        verify(mMockJni, never()).surfaceCreated(anyLong());
        verify(mMockJni, never()).surfaceDestroyed(anyLong());
        verify(mMockJni, never())
                .surfaceChanged(anyLong(), anyInt(), anyInt(), anyInt(), anyBoolean(), any());
    }

    @Test
    public void testDestroy_normalAndIdempotent() {
        mConstraints.supportsOpacity = true;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);

        compositorView.destroy();
        verify(mMockJni, times(1)).destroy(NATIVE_PTR);

        // Calling destroy() again should be a no-op and not call native destroy again
        compositorView.destroy();
        verify(mMockJni, times(1)).destroy(NATIVE_PTR);
    }

    @Test
    public void testDestroy_windowAndroidDestroyedThrows() {
        mConstraints.supportsOpacity = true;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);
        when(mWindowAndroid.getDestroyStack()).thenReturn(new RuntimeException("Window destroyed"));

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);

        assertThrows(IllegalStateException.class, compositorView::destroy);
    }

    @Test
    public void testRequestRender() {
        mConstraints.supportsOpacity = true;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);

        compositorView.requestRender();
        verify(mMockJni).setNeedsComposite(NATIVE_PTR);

        compositorView.destroy();
        compositorView.requestRender();
        verify(mMockJni, times(1)).setNeedsComposite(NATIVE_PTR);
    }

    @Test
    public void testRunOnNextFrame() {
        mConstraints.supportsOpacity = true;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);
        Runnable runnable = mock(Runnable.class);

        compositorView.runOnNextFrame(runnable);
        verify(mMockJni).runOnNextFrame(NATIVE_PTR, runnable);

        compositorView.destroy();
        compositorView.runOnNextFrame(runnable);
        verify(mMockJni, times(1)).runOnNextFrame(NATIVE_PTR, runnable);
    }

    @Test
    public void testSetAlpha() {
        mConstraints.supportsOpacity = true;
        when(mMockJni.shouldUseSurfaceView()).thenReturn(false);

        CompositorViewImpl compositorView =
                new CompositorViewImpl(mContext, mWindowAndroid, mConstraints);

        compositorView.setAlpha(0.5f);
        assertEquals(0.5f, compositorView.getView().getAlpha(), 0.01f);

        compositorView.destroy();
        compositorView.setAlpha(0.8f);
        // After destroy with mNativeCompositorViewImpl == 0, setAlpha returns early without
        // updating view alpha
        assertEquals(0.5f, compositorView.getView().getAlpha(), 0.01f);
    }
}
