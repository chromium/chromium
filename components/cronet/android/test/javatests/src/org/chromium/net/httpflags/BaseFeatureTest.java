// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.nio.charset.StandardCharsets;
import java.util.HashMap;

/** Tests {@link BaseFeature} */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class BaseFeatureTest {
    @Test
    @SmallTest
    public void testGetOverrides_emptyIfNoFlags() {
        assertThat(
                        BaseFeature.getOverrides(
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
                .isEqualTo(
                        BaseFeatureOverrides.newBuilder()
                                .putFeatureStates(
                                        "BaseFeatureOverride1",
                                        BaseFeatureOverrides.FeatureState.newBuilder()
                                                .setEnabled(true)
                                                .build())
                                .putFeatureStates(
                                        "BaseFeatureOverride2",
                                        BaseFeatureOverrides.FeatureState.newBuilder()
                                                .setEnabled(false)
                                                .build())
                                .build());
    }

    @Test
    @SmallTest
    public void testGetOverrides_throwsOnNonBooleanOverride() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put(
                BaseFeature.FLAG_PREFIX + "BaseFeatureOverride",
                new ResolvedFlags.Value("test value"));
        ResolvedFlags resolvedFlags = new ResolvedFlags(flags);

        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    BaseFeature.getOverrides(resolvedFlags);
                });
    }

    @Test
    @SmallTest
    public void testGetOverrides_returnsParamOverrides() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeatureName"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestBooleanParam",
                new ResolvedFlags.Value(true));
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeatureName"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestIntParam",
                new ResolvedFlags.Value(123));
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeatureName"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestFloatParam",
                new ResolvedFlags.Value(42.42f));
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeatureName"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestStringParam",
                new ResolvedFlags.Value("test_string_value"));
        ByteString byteString = ByteString.copyFrom(new byte[] {0, 1, 2, 42, -128, 127});
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeatureName"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestBytesParam",
                new ResolvedFlags.Value(byteString));

        assertThat(BaseFeature.getOverrides(new ResolvedFlags(flags)))
                .isEqualTo(
                        BaseFeatureOverrides.newBuilder()
                                .putFeatureStates(
                                        "TestBaseFeatureName",
                                        BaseFeatureOverrides.FeatureState.newBuilder()
                                                .putParams(
                                                        "TestBooleanParam",
                                                        ByteString.copyFrom(
                                                                "true", StandardCharsets.UTF_8))
                                                .putParams(
                                                        "TestIntParam",
                                                        ByteString.copyFrom(
                                                                "123", StandardCharsets.UTF_8))
                                                .putParams(
                                                        "TestFloatParam",
                                                        ByteString.copyFrom(
                                                                "42.42", StandardCharsets.UTF_8))
                                                .putParams(
                                                        "TestStringParam",
                                                        ByteString.copyFrom(
                                                                "test_string_value",
                                                                StandardCharsets.UTF_8))
                                                .putParams("TestBytesParam", byteString)
                                                .build())
                                .build());
    }

    @Test
    @SmallTest
    public void testGetOverrides_associatesParamsWithFeatures() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeature1Name"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestParam1",
                new ResolvedFlags.Value("test_value1"));
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeature2Name"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestParam2",
                new ResolvedFlags.Value("test_value2"));

        assertThat(BaseFeature.getOverrides(new ResolvedFlags(flags)))
                .isEqualTo(
                        BaseFeatureOverrides.newBuilder()
                                .putFeatureStates(
                                        "TestBaseFeature1Name",
                                        BaseFeatureOverrides.FeatureState.newBuilder()
                                                .putParams(
                                                        "TestParam1",
                                                        ByteString.copyFrom(
                                                                "test_value1",
                                                                StandardCharsets.UTF_8))
                                                .build())
                                .putFeatureStates(
                                        "TestBaseFeature2Name",
                                        BaseFeatureOverrides.FeatureState.newBuilder()
                                                .putParams(
                                                        "TestParam2",
                                                        ByteString.copyFrom(
                                                                "test_value2",
                                                                StandardCharsets.UTF_8))
                                                .build())
                                .build());
    }

    @Test
    @SmallTest
    public void testGetOverrides_combinesEnabledStateWithParam() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<String, ResolvedFlags.Value>();
        flags.put(BaseFeature.FLAG_PREFIX + "TestBaseFeatureName", new ResolvedFlags.Value(true));
        flags.put(
                BaseFeature.FLAG_PREFIX
                        + "TestBaseFeatureName"
                        + BaseFeature.PARAM_DELIMITER
                        + "TestParam",
                new ResolvedFlags.Value("test_value"));

        assertThat(BaseFeature.getOverrides(new ResolvedFlags(flags)))
                .isEqualTo(
                        BaseFeatureOverrides.newBuilder()
                                .putFeatureStates(
                                        "TestBaseFeatureName",
                                        BaseFeatureOverrides.FeatureState.newBuilder()
                                                .setEnabled(true)
                                                .putParams(
                                                        "TestParam",
                                                        ByteString.copyFrom(
                                                                "test_value",
                                                                StandardCharsets.UTF_8))
                                                .build())
                                .build());
    }
}
