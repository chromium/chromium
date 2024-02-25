// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.common.truth.Correspondence;
import com.google.protobuf.ByteString;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.util.Map;

/** Tests {@link ResolvedFlags} */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class ResolvedFlagsTest {
    private static final Correspondence<ResolvedFlags.Value, String> FLAG_STRING_VALUE_EQUALS =
            Correspondence.transforming(
                    ResolvedFlags.Value::getStringValue, "has a string value of");

    private static FlagValue.ConstrainedValue.Builder stringConstrainedValue(String value) {
        return FlagValue.ConstrainedValue.newBuilder().setStringValue(value);
    }

    private static Flags singleFlag(String flagName, FlagValue.Builder flagValue) {
        return Flags.newBuilder().putFlags(flagName, flagValue.build()).build();
    }

    @Test
    @SmallTest
    public void testResolve_emptyOnEmptyProto() {
        assertThat(
                        ResolvedFlags.resolve(Flags.newBuilder().build(), "test_app_id", "1.2.3.4")
                                .flags())
                .isEmpty();
    }

    @Test
    @SmallTest
    public void testResolve_returnsAllFlags() {
        assertThat(
                        ResolvedFlags.resolve(
                                        Flags.newBuilder()
                                                .putFlags(
                                                        "test_flag_1",
                                                        FlagValue.newBuilder()
                                                                .addConstrainedValues(
                                                                        stringConstrainedValue(
                                                                                "test_flag_1_value"))
                                                                .build())
                                                .putFlags(
                                                        "test_flag_2",
                                                        FlagValue.newBuilder()
                                                                .addConstrainedValues(
                                                                        stringConstrainedValue(
                                                                                "test_flag_2_value"))
                                                                .build())
                                                .build(),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .comparingValuesUsing(FLAG_STRING_VALUE_EQUALS)
                .containsExactly(
                        "test_flag_1", "test_flag_1_value", "test_flag_2", "test_flag_2_value");
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagWithEmptyConstrainedValue() {
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(
                                                                FlagValue.ConstrainedValue
                                                                        .newBuilder())),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .isEmpty();
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagWithNoConstrainedValues() {
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag("test_flag", FlagValue.newBuilder()),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .isEmpty();
    }

    @Test
    @SmallTest
    public void testResolve_returnsFlagThatMatchesAppId() {
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(
                                                                stringConstrainedValue(
                                                                                "test_flag_value")
                                                                        .setAppId("test_app_id"))),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .comparingValuesUsing(FLAG_STRING_VALUE_EQUALS)
                .containsExactly("test_flag", "test_flag_value");
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagThatDoesNotMatchAppId() {
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(
                                                                stringConstrainedValue(
                                                                                "test_flag_value")
                                                                        .setAppId(
                                                                                "nonmatching_app_id"))),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .isEmpty();
    }

    @Test
    @SmallTest
    public void testResolve_throwsOnEmptyCronetVersion() {
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    ResolvedFlags.resolve(Flags.newBuilder().build(), "test_app_id", "");
                });
    }

    @Test
    @SmallTest
    public void testResolve_throwsOnInvalidCronetVersion() {
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    ResolvedFlags.resolve(Flags.newBuilder().build(), "test_app_id", "1.2.a.4");
                });
    }

    @Test
    @SmallTest
    public void testResolve_throwsOnEmptyMinVersion() {
        Flags flags =
                singleFlag(
                        "test_flag",
                        FlagValue.newBuilder()
                                .addConstrainedValues(
                                        stringConstrainedValue("test_flag_value")
                                                .setMinVersion("")));
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    ResolvedFlags.resolve(flags, "test_app_id", "1.2.3.4");
                });
    }

    @Test
    @SmallTest
    public void testResolve_throwsOnInvalidMinVersion() {
        Flags flags =
                singleFlag(
                        "test_flag",
                        FlagValue.newBuilder()
                                .addConstrainedValues(
                                        stringConstrainedValue("test_flag_value")
                                                .setMinVersion("1.2.a.4")));
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    ResolvedFlags.resolve(flags, "test_app_id", "1.2.3.4");
                });
    }

    private void checkMinVersion(String cronetVersion, String minVersion, boolean expectMatch) {
        Map<String, ResolvedFlags.Value> flags =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        stringConstrainedValue("test_flag_value")
                                                                .setMinVersion(minVersion))),
                                "test_app_id",
                                cronetVersion)
                        .flags();
        if (expectMatch) {
            assertThat(flags)
                    .comparingValuesUsing(FLAG_STRING_VALUE_EQUALS)
                    .containsExactly("test_flag", "test_flag_value");
        } else {
            assertThat(flags).isEmpty();
        }
    }

    @Test
    @SmallTest
    public void testResolve_returnsFlagIfCronetVersionIsSameAsMinVersion() {
        checkMinVersion("5.6.7.8", "5.6.7.8", true);
    }

    @Test
    @SmallTest
    public void testResolve_returnsFlagIfCronetPatchVersionIsHigherThanMinPatchVersion() {
        checkMinVersion("5.6.7.9", "5.6.7.8", true);
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagIfCronetPatchVersionIsLowerThanMinPatchVersion() {
        checkMinVersion("5.6.7.7", "5.6.7.8", false);
    }

    @Test
    @SmallTest
    public void testResolve_returnsFlagIfCronetMajorVersionIsHigherThanMinMajorVersion() {
        checkMinVersion("6.6.7.8", "5.6.7.8", true);
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagIfCronetPatchVersionIsLowerThanMinMajorVersion() {
        checkMinVersion("4.6.7.7", "5.6.7.8", false);
    }

    @Test
    @SmallTest
    public void testResolve_returnsFlagOnCronetMajorVersionMatch() {
        checkMinVersion("5.0.0.0", "5", true);
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagOnLowerCronetMajorVersion() {
        checkMinVersion("4.9.9.9", "5", false);
    }

    @Test
    @SmallTest
    public void testResolve_returnsOnlyMatchingConstrainedValue() {
        FlagValue.ConstrainedValue matching_value =
                stringConstrainedValue("matching_test_flag_value").setAppId("test_app_id").build();
        FlagValue.ConstrainedValue nonmatching_value =
                stringConstrainedValue("nonmatching_test_flag_value")
                        .setAppId("nonmatching_app_id")
                        .build();

        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(matching_value)
                                                        .addConstrainedValues(nonmatching_value)),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .comparingValuesUsing(FLAG_STRING_VALUE_EQUALS)
                .containsExactly("test_flag", "matching_test_flag_value");
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(nonmatching_value)
                                                        .addConstrainedValues(matching_value)),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .comparingValuesUsing(FLAG_STRING_VALUE_EQUALS)
                .containsExactly("test_flag", "matching_test_flag_value");
    }

    @Test
    @SmallTest
    public void testResolve_returnsFirstMatchingConstrainedValue() {
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(
                                                                stringConstrainedValue(
                                                                        "test_flag_value_1"))
                                                        .addConstrainedValues(
                                                                stringConstrainedValue(
                                                                        "test_flag_value_2"))),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .comparingValuesUsing(FLAG_STRING_VALUE_EQUALS)
                .containsExactly("test_flag", "test_flag_value_1");
    }

    @Test
    @SmallTest
    public void testResolve_doesNotReturnFlagIfMatchingValueIsEmpty() {
        assertThat(
                        ResolvedFlags.resolve(
                                        singleFlag(
                                                "test_flag",
                                                FlagValue.newBuilder()
                                                        .addConstrainedValues(
                                                                FlagValue.ConstrainedValue
                                                                        .newBuilder())
                                                        .addConstrainedValues(
                                                                stringConstrainedValue(
                                                                        "test_flag_value_should_be_skipped"))),
                                        "test_app_id",
                                        "1.2.3.4")
                                .flags())
                .isEmpty();
    }

    @Test
    @SmallTest
    public void testResolve_returnsFalseValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setBoolValue(false))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.BOOL);
        assertThat(value.getBoolValue()).isFalse();
    }

    @Test
    @SmallTest
    public void testResolve_returnsTrueValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setBoolValue(true))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.BOOL);
        assertThat(value.getBoolValue()).isTrue();
    }

    @Test
    @SmallTest
    public void testResolve_returnsZeroIntValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setIntValue(0))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.INT);
        assertThat(value.getIntValue()).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testResolve_returnsNonZeroIntValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setIntValue(42))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.INT);
        assertThat(value.getIntValue()).isEqualTo(42);
    }

    @Test
    @SmallTest
    public void testResolve_returnsZeroFloatValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setFloatValue(0))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.FLOAT);
        assertThat(value.getFloatValue()).isEqualTo(0f);
    }

    @Test
    @SmallTest
    public void testResolve_returnsNonZeroFloatValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setFloatValue(42))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.FLOAT);
        assertThat(value.getFloatValue()).isEqualTo(42f);
    }

    @Test
    @SmallTest
    public void testResolve_returnsEmptyStringValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setStringValue(""))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.STRING);
        assertThat(value.getStringValue()).isEqualTo("");
    }

    @Test
    @SmallTest
    public void testResolve_returnsNonEmptyStringValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setStringValue(
                                                                        "test_string_value"))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.STRING);
        assertThat(value.getStringValue()).isEqualTo("test_string_value");
    }

    @Test
    @SmallTest
    public void testResolve_returnsEmptyBytesValue() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setBytesValue(ByteString.EMPTY))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.BYTES);
        assertThat(value.getBytesValue()).isEqualTo(ByteString.EMPTY);
    }

    @Test
    @SmallTest
    public void testResolve_returnsNonEmptyBytesValue() {
        ByteString byteString = ByteString.copyFrom(new byte[] {0, 1, 2, 42, -128, 127});
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setBytesValue(byteString))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value).isNotNull();
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.BYTES);
        assertThat(value.getBytesValue()).isEqualTo(byteString);
    }

    @Test
    @SmallTest
    public void testResolve_throwsOnWrongTypeAccess() {
        ResolvedFlags.Value value =
                ResolvedFlags.resolve(
                                singleFlag(
                                        "test_flag",
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setStringValue("test_string"))),
                                "test_app_id",
                                "1.2.3.4")
                        .flags()
                        .get("test_flag");
        assertThat(value.getType()).isEqualTo(ResolvedFlags.Value.Type.STRING);
        assertThrows(
                IllegalStateException.class,
                () -> {
                    value.getIntValue();
                });
    }
}
