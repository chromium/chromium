// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link CompositorModelChangeProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CompositorModelChangeProcessorUnitTest {
    private static final PropertyModel.WritableBooleanPropertyKey PROPERTY_CHANGED =
            new PropertyModel.WritableBooleanPropertyKey();

    private static final PropertyModel.WritableBooleanPropertyKey PROPERTY_EXCLUDED =
            new PropertyModel.WritableBooleanPropertyKey();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private final CallbackHelper mRequestRenderCallbackHelper = new CallbackHelper();

    @Mock private SceneLayer mView;
    @Mock private PropertyModelChangeProcessor.ViewBinder mViewBinder;

    private CompositorModelChangeProcessor.FrameRequestSupplier mFrameSupplier;
    private CompositorModelChangeProcessor mCompositorMCP;
    private PropertyModel mModel;
    private final AtomicBoolean mPropertyChangedValue = new AtomicBoolean(false);

    @Before
    public void setUp() {
        mFrameSupplier =
                new CompositorModelChangeProcessor.FrameRequestSupplier(
                        mRequestRenderCallbackHelper::notifyCalled);
        mModel = new PropertyModel(PROPERTY_CHANGED, PROPERTY_EXCLUDED);

        Set<PropertyKey> exclusions = new HashSet();
        exclusions.add(PROPERTY_EXCLUDED);
        mCompositorMCP =
                CompositorModelChangeProcessor.create(
                        mModel, mView, mViewBinder, mFrameSupplier, false, exclusions);
    }

    @Test
    public void testBindAndRequestFrame() throws TimeoutException {
        int callCount = mRequestRenderCallbackHelper.getCallCount();
        mModel.set(PROPERTY_CHANGED, mPropertyChangedValue.getAndSet(!mPropertyChangedValue.get()));
        mRequestRenderCallbackHelper.waitForCallback(callCount, 1);

        mFrameSupplier.set(System.currentTimeMillis());
        verify(mViewBinder).bind(eq(mModel), eq(mView), eq(null));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.MVC_UPDATE_VIEW_WHEN_MODEL_CHANGED})
    public void testBindAndNoRequestFrame() {
        int callCount = mRequestRenderCallbackHelper.getCallCount();
        mFrameSupplier.set(System.currentTimeMillis());

        verify(mViewBinder).bind(eq(mModel), eq(mView), eq(null));
        Assert.assertEquals(
                "A render should not have been requested!",
                callCount,
                mRequestRenderCallbackHelper.getCallCount());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.MVC_UPDATE_VIEW_WHEN_MODEL_CHANGED})
    public void testNoBindAndNoRequestFrameOnModelUnchanged() throws TimeoutException {
        int callCount = mRequestRenderCallbackHelper.getCallCount();
        mFrameSupplier.set(System.currentTimeMillis());
        verify(mViewBinder, never()).bind(any(), any(), any());

        Assert.assertEquals(
                "A render should not have been requested!",
                callCount,
                mRequestRenderCallbackHelper.getCallCount());
    }

    @Test
    public void testRequestFrameAndNoBindOnPropertyChanged() throws TimeoutException {
        int callCount = mRequestRenderCallbackHelper.getCallCount();
        mModel.set(PROPERTY_CHANGED, mPropertyChangedValue.getAndSet(!mPropertyChangedValue.get()));
        mRequestRenderCallbackHelper.waitForCallback(callCount, 1);

        verify(mViewBinder, never()).bind(any(), any(), any());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.MVC_UPDATE_VIEW_WHEN_MODEL_CHANGED})
    public void testMCPWithExclusions() {
        int callCount = mRequestRenderCallbackHelper.getCallCount();
        mModel.set(
                PROPERTY_EXCLUDED, mPropertyChangedValue.getAndSet(!mPropertyChangedValue.get()));

        mFrameSupplier.set(System.currentTimeMillis());
        verify(mViewBinder).bind(eq(mModel), eq(mView), eq(null));
        Assert.assertEquals(
                "A render should not have been requested!",
                callCount,
                mRequestRenderCallbackHelper.getCallCount());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.MVC_UPDATE_VIEW_WHEN_MODEL_CHANGED})
    public void testMCPWithExclusionsNoBind() {
        int callCount = mRequestRenderCallbackHelper.getCallCount();
        mModel.set(
                PROPERTY_EXCLUDED, mPropertyChangedValue.getAndSet(!mPropertyChangedValue.get()));

        mFrameSupplier.set(System.currentTimeMillis());
        verify(mViewBinder, never()).bind(any(), any(), any());
        Assert.assertEquals(
                "A render should not have been requested!",
                callCount,
                mRequestRenderCallbackHelper.getCallCount());
    }
}
