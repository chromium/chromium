// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

/** Tests {@link ExperimentalOptionsFlags} */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class ExperimentalOptionsFlagsTest {
    @Test
    @SmallTest
    public void testApplyOverrides_emptyFlags_returnsOriginal() {
        assertThat(ExperimentalOptionsFlags.applyOverrides(null, Collections.emptyMap())).isNull();
        assertThat(ExperimentalOptionsFlags.applyOverrides("", Collections.emptyMap()))
                .isEqualTo("");
        assertThat(
                        ExperimentalOptionsFlags.applyOverrides(
                                "{\"QUIC\":{\"test\":1}}", Collections.emptyMap()))
                .isEqualTo("{\"QUIC\":{\"test\":1}}");
    }

    @Test
    @SmallTest
    public void testApplyOverrides_noMatchingFlags_returnsOriginal() throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put("NotAnExperimentalOption", new ResolvedFlags.Value("test value"));
        flags.put("ChromiumBaseFeature_Foo", new ResolvedFlags.Value(true));

        assertThat(ExperimentalOptionsFlags.applyOverrides("{\"QUIC\":{\"test\":1}}", flags))
                .isEqualTo("{\"QUIC\":{\"test\":1}}");
    }

    @Test
    @SmallTest
    public void testApplyOverrides_alwaysOverride_nestedSection_setsNewKey() throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds",
                new ResolvedFlags.Value(60));

        String result = ExperimentalOptionsFlags.applyOverrides(null, flags);
        JSONObject json = new JSONObject(result);
        assertThat(json.getJSONObject("QUIC").getInt("idle_connection_timeout_seconds"))
                .isEqualTo(60);
    }

    @Test
    @SmallTest
    public void testApplyOverrides_alwaysOverride_nestedSection_replacesExistingKey()
            throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds",
                new ResolvedFlags.Value(60));

        String baseJson =
                "{\"QUIC\":{\"idle_connection_timeout_seconds\":30,\"other_param\":\"value\"}}";
        String result = ExperimentalOptionsFlags.applyOverrides(baseJson, flags);
        JSONObject json = new JSONObject(result);
        assertThat(json.getJSONObject("QUIC").getInt("idle_connection_timeout_seconds"))
                .isEqualTo(60);
        assertThat(json.getJSONObject("QUIC").getString("other_param")).isEqualTo("value");
    }

    @Test
    @SmallTest
    public void testParseOverride_valid() {
        ExperimentalOptionsFlags.ParsedOverride parsed =
                ExperimentalOptionsFlags.parseOverride(
                        "CronetExperimentalOption_QUIC_KEY_idle_timeout",
                        new ResolvedFlags.Value(60));
        assertThat(parsed).isNotNull();
        assertThat(parsed.mSectionName).isEqualTo("QUIC");
        assertThat(parsed.mOptionName).isEqualTo("idle_timeout");
        assertThat(parsed.mValue).isEqualTo(60);
    }

    @Test
    @SmallTest
    public void testParseOverride_notMatchingPrefix_returnsNull() {
        ResolvedFlags.Value value = new ResolvedFlags.Value(true);
        assertThat(ExperimentalOptionsFlags.parseOverride("NotAnExperimentalOption", value))
                .isNull();
    }

    @Test
    @SmallTest
    public void testParseOverride_missingDelimiter_throws() {
        ResolvedFlags.Value value = new ResolvedFlags.Value(true);
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        ExperimentalOptionsFlags.parseOverride(
                                "CronetExperimentalOption_disable_ipv6_on_wifi", value));
    }

    @Test
    @SmallTest
    public void testParseOverride_emptyRemaining_throws() {
        ResolvedFlags.Value value = new ResolvedFlags.Value(true);
        assertThrows(
                IllegalArgumentException.class,
                () -> ExperimentalOptionsFlags.parseOverride("CronetExperimentalOption_", value));
    }

    @Test
    @SmallTest
    public void testParseOverride_emptySectionName_throws() {
        ResolvedFlags.Value value = new ResolvedFlags.Value(true);
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        ExperimentalOptionsFlags.parseOverride(
                                "CronetExperimentalOption__KEY_option_name", value));
    }

    @Test
    @SmallTest
    public void testParseOverride_emptyOptionName_throws() {
        ResolvedFlags.Value value = new ResolvedFlags.Value(true);
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        ExperimentalOptionsFlags.parseOverride(
                                "CronetExperimentalOption_QUIC_KEY_", value));
    }

    @Test
    @SmallTest
    public void testParseOverride_bytesType_throws() {
        ResolvedFlags.Value value =
                new ResolvedFlags.Value(ByteString.copyFrom("utf8_data", StandardCharsets.UTF_8));
        assertThrows(
                IllegalArgumentException.class,
                () ->
                        ExperimentalOptionsFlags.parseOverride(
                                "CronetExperimentalOption_Test_KEY_bytes_val", value));
    }

    @Test
    @SmallTest
    public void testApplyOverrides_allTypesSupported() throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put("CronetExperimentalOption_Test_KEY_bool_val", new ResolvedFlags.Value(true));
        flags.put("CronetExperimentalOption_Test_KEY_int_val", new ResolvedFlags.Value(42));
        flags.put("CronetExperimentalOption_Test_KEY_float_val", new ResolvedFlags.Value(3.14f));
        flags.put("CronetExperimentalOption_Test_KEY_str_val", new ResolvedFlags.Value("hello"));

        String result = ExperimentalOptionsFlags.applyOverrides(null, flags);
        JSONObject json = new JSONObject(result);
        JSONObject testSection = json.getJSONObject("Test");

        assertThat(testSection.getBoolean("bool_val")).isTrue();
        assertThat(testSection.getInt("int_val")).isEqualTo(42);
        assertThat(testSection.getDouble("float_val")).isWithin(0.01).of(3.14);
        assertThat(testSection.getString("str_val")).isEqualTo("hello");
    }

    @Test
    @SmallTest
    public void testApplyOverrides_bytesType_throws() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_Test_KEY_bytes_val",
                new ResolvedFlags.Value(ByteString.copyFrom("utf8_data", StandardCharsets.UTF_8)));

        assertThrows(
                IllegalArgumentException.class,
                () -> ExperimentalOptionsFlags.applyOverrides(null, flags));
    }

    @Test
    @SmallTest
    public void testApplyOverrides_malformedFlag_throws() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put("CronetExperimentalOption_InvalidFlag", new ResolvedFlags.Value(true));

        assertThrows(
                IllegalArgumentException.class,
                () -> ExperimentalOptionsFlags.applyOverrides(null, flags));
    }

    @Test
    @SmallTest
    public void testApplyOverrides_jsonString_treatedAsRawString() throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_NetworkErrorLogging_KEY_preloaded_report_to_headers",
                new ResolvedFlags.Value("[{\"group\":\"nel\",\"max_age\":86400}]"));

        String result = ExperimentalOptionsFlags.applyOverrides(null, flags);
        JSONObject json = new JSONObject(result);
        JSONObject nel = json.getJSONObject("NetworkErrorLogging");
        assertThat(nel.getString("preloaded_report_to_headers"))
                .isEqualTo("[{\"group\":\"nel\",\"max_age\":86400}]");
    }

    @Test
    @SmallTest
    public void testApplyOverrides_malformedBaseJson_returnsOriginalUntouched() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds",
                new ResolvedFlags.Value(45));

        String result = ExperimentalOptionsFlags.applyOverrides("invalid { json", flags);
        assertThat(result).isEqualTo("invalid { json");
    }

    @Test
    @SmallTest
    public void testApplyOverrides_sectionNotJsonObject_throws() {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds",
                new ResolvedFlags.Value(45));

        assertThrows(
                IllegalArgumentException.class,
                () ->
                        ExperimentalOptionsFlags.applyOverrides(
                                "{\"QUIC\":\"not_a_json_object\"}", flags));
    }

    @Test
    @SmallTest
    public void testApplyOverrides_multipleKeysInSameSection() throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds",
                new ResolvedFlags.Value(60));
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_connection_options",
                new ResolvedFlags.Value("FLAG"));

        String result = ExperimentalOptionsFlags.applyOverrides(null, flags);
        JSONObject json = new JSONObject(result);
        JSONObject quic = json.getJSONObject("QUIC");
        assertThat(quic.getInt("idle_connection_timeout_seconds")).isEqualTo(60);
        assertThat(quic.getString("connection_options")).isEqualTo("FLAG");
    }

    @Test
    @SmallTest
    public void testApplyOverrides_multipleSections() throws Exception {
        HashMap<String, ResolvedFlags.Value> flags = new HashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_connection_options",
                new ResolvedFlags.Value("FLAG"));
        flags.put("CronetExperimentalOption_AsyncDNS_KEY_enable", new ResolvedFlags.Value(true));

        String result = ExperimentalOptionsFlags.applyOverrides(null, flags);
        JSONObject json = new JSONObject(result);
        assertThat(json.getJSONObject("QUIC").getString("connection_options")).isEqualTo("FLAG");
        assertThat(json.getJSONObject("AsyncDNS").getBoolean("enable")).isTrue();
    }

    @Test
    @SmallTest
    public void testApplyOverrides_matchesManuallyConstructedJson() throws Exception {
        JSONObject quic = new JSONObject();
        quic.put("idle_connection_timeout_seconds", 60);
        quic.put("connection_options", "FLAG");
        JSONObject manualOptions = new JSONObject();
        manualOptions.put("QUIC", quic);

        Map<String, ResolvedFlags.Value> flags = new LinkedHashMap<>();
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_idle_connection_timeout_seconds",
                new ResolvedFlags.Value(60));
        flags.put(
                "CronetExperimentalOption_QUIC_KEY_connection_options",
                new ResolvedFlags.Value("FLAG"));

        String flagOptions = ExperimentalOptionsFlags.applyOverrides(null, flags);
        assertThat(flagOptions).isEqualTo(manualOptions.toString());
    }
}
