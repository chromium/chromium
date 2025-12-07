// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Build;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.content_capture.ContentCaptureMetadataProto.ContentCaptureMetadata;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link OnscreenContentProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.R)
public class OnscreenContentProviderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnscreenContentProvider.Natives mOnscreenContentProviderJni;

    @Mock private View mView;
    @Mock private PlatformContentCaptureController mMockController;
    @Mock private Context mContext;
    @Mock private WebContents mWebContents;

    private OnscreenContentProvider mProvider;

    @Before
    public void setUp() {
        OnscreenContentProviderJni.setInstanceForTesting(mOnscreenContentProviderJni);
        ReflectionHelpers.setStaticField(
                PlatformContentCaptureController.class,
                "sContentCaptureController",
                mMockController);
        mProvider = new OnscreenContentProvider(mContext, mView, mWebContents);
    }

    @After
    public void tearDown() {
        // Reset the singleton to avoid leaking state
        ReflectionHelpers.setStaticField(
                PlatformContentCaptureController.class, "sContentCaptureController", null);
    }

    @Test
    public void testDidUpdateSensitivityScore() {
        String url = "https://example.com";
        float sensitivityScore = 0.5f;

        ReflectionHelpers.callInstanceMethod(
                mProvider,
                "didUpdateSensitivityScore",
                ReflectionHelpers.ClassParameter.from(String.class, url),
                ReflectionHelpers.ClassParameter.from(float.class, sensitivityScore));

        verify(mMockController).shareData(eq(url), any(ContentCaptureMetadata.class));
    }
}
