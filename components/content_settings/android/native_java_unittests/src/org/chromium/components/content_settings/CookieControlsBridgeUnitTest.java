// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import org.jni_zero.CalledByNative;
import org.junit.Assert;

import java.util.List;

public class CookieControlsBridgeUnitTest {
    @CalledByNative
    private CookieControlsBridgeUnitTest() {}

    @CalledByNative
    public void testTpList(List<CookieControlsBridge.TrackingProtectionFeature> features) {
        Assert.assertNotNull(features);
        Assert.assertEquals(1, features.size());
        var feature = features.get(0);
        Assert.assertNotNull(feature);
        Assert.assertEquals(TrackingProtectionFeatureType.THIRD_PARTY_COOKIES, feature.featureType);
        Assert.assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, feature.enforcement);
        Assert.assertEquals(TrackingProtectionBlockingStatus.ALLOWED, feature.status);
    }
}
