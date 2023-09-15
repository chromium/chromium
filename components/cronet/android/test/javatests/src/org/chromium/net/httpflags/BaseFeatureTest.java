// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.util.HashMap;

/**
 * Tests {@link BaseFeature}
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class BaseFeatureTest {
    @Test
    @SmallTest
    public void testGetOverrides_emptyIfNoFlags() {
        assertThat(BaseFeature.getOverrides(
                           new ResolvedFlags(new HashMap<String, ResolvedFlags.Value>())))
                .isEqualTo(BaseFeatureOverrides.newBuilder().build());
    }

    @Test
    @SmallTest
    public void testGetOverrides_emptyIfNoBaseFeatureOverrideFlags() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put("NotABaseFeatureOverride", new ResolvedFlags.Value("test value"));

        assertThat(BaseFeature.getOverrides(new ResolvedFlags(flags)))
                .isEqualTo(BaseFeatureOverrides.newBuilder().build());
    }

    @Test
    @SmallTest
    public void testGetOverrides_returnsOverrides() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put("NotABaseFeatureOverride1", new ResolvedFlags.Value(true));
        flags.put(BaseFeature.FLAG_PREFIX + "BaseFeatureOverride1", new ResolvedFlags.Value(true));
        flags.put("NotABaseFeatureOverride2", new ResolvedFlags.Value(false));
        flags.put(BaseFeature.FLAG_PREFIX + "BaseFeatureOverride2", new ResolvedFlags.Value(false));
        flags.put("NotABaseFeatureOverride3", new ResolvedFlags.Value(true));

        assertThat(BaseFeature.getOverrides(new ResolvedFlags(flags)))
                .isEqualTo(BaseFeatureOverrides.newBuilder()
                                   .putOverrides("BaseFeatureOverride1", true)
                                   .putOverrides("BaseFeatureOverride2", false)
                                   .build());
    }

    @Test
    @SmallTest
    public void testGetOverrides_throwsOnNonBooleanOverride() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put(BaseFeature.FLAG_PREFIX + "BaseFeatureOverride",
                new ResolvedFlags.Value("test value"));
        ResolvedFlags resolvedFlags = new ResolvedFlags(flags);

        assertThrows(
                IllegalArgumentException.class, () -> { BaseFeature.getOverrides(resolvedFlags); });
    }
}
