// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated using a Google-internal version of
// https://cs.android.com/android/platform/superproject/main/+/main:frameworks/proto_logging/stats/stats_log_api_gen/
// fed with Google-internal Cronet atom proto definitions. Google employees should refer to
// go/extend-cronet-telemetry.

package org.chromium.net.telemetry;

import android.os.Build;
import android.util.StatsEvent;
import android.util.StatsLog;

import androidx.annotation.RequiresApi;

/** Utility class for logging statistics events. */
public final class CronetStatsLog {
    // Constants for atom codes.

    /**
     * CronetEngineCreated cronet_engine_created<br>
     * Usage: StatsLog.write(StatsLog.CRONET_ENGINE_CREATED, long engine_instance_ref, int
     * major_version, int minor_version, int build_version, int patch_version, int source, boolean
     * enable_brotli, boolean enable_http2, int http_cache_mode, boolean
     * enable_public_key_pinning_bypass_for_local_trust_anchors, boolean enable_quic, boolean
     * enable_network_quality_estimator, int thread_priority, java.lang.String
     * experimental_options_quic_connection_options, int
     * experimental_options_quic_store_server_configs_in_properties, int
     * experimental_options_quic_max_server_configs_stored_in_properties, int
     * experimental_options_quic_idle_connection_timeout_seconds, int
     * experimental_options_quic_goaway_sessions_on_ip_change, int
     * experimental_options_quic_close_sessions_on_ip_change, int
     * experimental_options_quic_migrate_sessions_on_network_change_v2, int
     * experimental_options_quic_migrate_sessions_early_v2, int
     * experimental_options_quic_quic_disable_bidirectional_streams, int
     * experimental_options_quic_max_time_before_crypto_handshake_seconds, int
     * experimental_options_quic_max_idle_time_before_crypto_handshake_seconds, int
     * experimental_options_quic_enable_socket_recv_optimization, int
     * experimental_options_asyncdns_enable, int experimental_options_staledns_enable, int
     * experimental_options_staledns_delay_ms, int
     * experimental_options_staledns_max_expired_time_ms, int
     * experimental_options_staledns_max_stale_uses, int
     * experimental_options_staledns_allow_other_network, int
     * experimental_options_staledns_persist_to_disk, int
     * experimental_options_staledns_persist_delay_ms, int
     * experimental_options_staledns_use_stale_on_name_not_resolved, int
     * experimental_options_disable_ipv6_on_wifi, long cronet_initialization_ref);<br>
     */
    public static final int CRONET_ENGINE_CREATED = 703;

    /**
     * CronetTrafficReported cronet_traffic_reported<br>
     * Usage: StatsLog.write(StatsLog.CRONET_TRAFFIC_REPORTED, long engine_instance_ref, int
     * request_headers_size, int request_body_size, int response_headers_size, int
     * response_body_size, int http_status_code, long negotiated_protocol_hash, int
     * headers_latency_millis, int overall_latency_millis, boolean connection_migration_attempted,
     * boolean connection_migration_successful, int samples_rate_limited, int terminal_state, int
     * nonfinal_user_callback_exception_count, long total_idle_time_millis, long
     * total_user_executor_execute_latency_millis, int read_count, int on_upload_read_count, int
     * is_bidi_stream, int final_user_callback_threw, int uid, int cronet_internal_error_code, int
     * quic_detailed_error_code, int quic_connection_close_source, int failure_reason, int
     * is_socket_reused);<br>
     */
    public static final int CRONET_TRAFFIC_REPORTED = 704;

    /**
     * CronetEngineBuilderInitialized cronet_engine_builder_initialized<br>
     * Usage: StatsLog.write(StatsLog.CRONET_ENGINE_BUILDER_INITIALIZED, long
     * cronet_initialization_ref, int author, int engine_builder_created_latency_millis, int source,
     * int creation_successful, int api_major_version, int api_minor_version, int api_build_version,
     * int api_patch_version, int impl_major_version, int impl_minor_version, int
     * impl_build_version, int impl_patch_version, int uid);<br>
     */
    public static final int CRONET_ENGINE_BUILDER_INITIALIZED = 762;

    /**
     * CronetInitialized cronet_initialized<br>
     * Usage: StatsLog.write(StatsLog.CRONET_INITIALIZED, long cronet_initialization_ref, int
     * engine_creation_latency_millis, int engine_async_latency_millis, int
     * http_flags_latency_millis, int http_flags_successful, long[] http_flags_names, long[]
     * http_flags_values);<br>
     */
    public static final int CRONET_INITIALIZED = 764;

    // Constants for enum values.

    // Values for CronetEngineCreated.source
    public static final int CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_UNSPECIFIED = 0;
    public static final int CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_STATICALLY_LINKED = 1;
    public static final int CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_GMSCORE_DYNAMITE = 2;
    public static final int CRONET_ENGINE_CREATED__SOURCE__CRONET_SOURCE_FALLBACK = 3;

    // Values for CronetEngineCreated.http_cache_mode
    public static final int CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_MODE_UNSPECIFIED = 0;
    public static final int CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_DISABLED = 1;
    public static final int CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_DISK = 2;
    public static final int CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_DISK_NO_HTTP = 3;
    public static final int CRONET_ENGINE_CREATED__HTTP_CACHE_MODE__HTTP_CACHE_IN_MEMORY = 4;

    // Values for CronetEngineCreated.experimental_options_quic_store_server_configs_in_properties
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_STORE_SERVER_CONFIGS_IN_PROPERTIES__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_STORE_SERVER_CONFIGS_IN_PROPERTIES__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_STORE_SERVER_CONFIGS_IN_PROPERTIES__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_quic_goaway_sessions_on_ip_change
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_GOAWAY_SESSIONS_ON_IP_CHANGE__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_GOAWAY_SESSIONS_ON_IP_CHANGE__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_GOAWAY_SESSIONS_ON_IP_CHANGE__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_quic_close_sessions_on_ip_change
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_CLOSE_SESSIONS_ON_IP_CHANGE__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_CLOSE_SESSIONS_ON_IP_CHANGE__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_CLOSE_SESSIONS_ON_IP_CHANGE__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for
    // CronetEngineCreated.experimental_options_quic_migrate_sessions_on_network_change_v2
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_MIGRATE_SESSIONS_ON_NETWORK_CHANGE_V2__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_MIGRATE_SESSIONS_ON_NETWORK_CHANGE_V2__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_MIGRATE_SESSIONS_ON_NETWORK_CHANGE_V2__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_quic_migrate_sessions_early_v2
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_MIGRATE_SESSIONS_EARLY_V2__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_MIGRATE_SESSIONS_EARLY_V2__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_MIGRATE_SESSIONS_EARLY_V2__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_quic_quic_disable_bidirectional_streams
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_QUIC_DISABLE_BIDIRECTIONAL_STREAMS__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_QUIC_DISABLE_BIDIRECTIONAL_STREAMS__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_QUIC_DISABLE_BIDIRECTIONAL_STREAMS__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_quic_enable_socket_recv_optimization
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_ENABLE_SOCKET_RECV_OPTIMIZATION__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_ENABLE_SOCKET_RECV_OPTIMIZATION__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_QUIC_ENABLE_SOCKET_RECV_OPTIMIZATION__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_asyncdns_enable
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_ASYNCDNS_ENABLE__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_ASYNCDNS_ENABLE__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_ASYNCDNS_ENABLE__OPTIONAL_BOOLEAN_FALSE = 2;

    // Values for CronetEngineCreated.experimental_options_staledns_enable
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_ENABLE__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_ENABLE__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_ENABLE__OPTIONAL_BOOLEAN_FALSE = 2;

    // Values for CronetEngineCreated.experimental_options_staledns_allow_other_network
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_ALLOW_OTHER_NETWORK__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_ALLOW_OTHER_NETWORK__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_ALLOW_OTHER_NETWORK__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_staledns_persist_to_disk
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_PERSIST_TO_DISK__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_PERSIST_TO_DISK__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_PERSIST_TO_DISK__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_staledns_use_stale_on_name_not_resolved
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_USE_STALE_ON_NAME_NOT_RESOLVED__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_USE_STALE_ON_NAME_NOT_RESOLVED__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_STALEDNS_USE_STALE_ON_NAME_NOT_RESOLVED__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetEngineCreated.experimental_options_disable_ipv6_on_wifi
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_DISABLE_IPV6_ON_WIFI__OPTIONAL_BOOLEAN_UNSET =
                    0;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_DISABLE_IPV6_ON_WIFI__OPTIONAL_BOOLEAN_TRUE =
                    1;
    public static final int
            CRONET_ENGINE_CREATED__EXPERIMENTAL_OPTIONS_DISABLE_IPV6_ON_WIFI__OPTIONAL_BOOLEAN_FALSE =
                    2;

    // Values for CronetTrafficReported.request_headers_size
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_UNSPECIFIED =
                    0;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_UNDER_ONE_KIB =
                    1;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_ONE_TO_TEN_KIB =
                    2;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_TEN_TO_TWENTY_FIVE_KIB =
                    3;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_TWENTY_FIVE_TO_FIFTY_KIB =
                    4;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_FIFTY_TO_HUNDRED_KIB =
                    5;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_OVER_HUNDRED_KIB =
                    6;

    // Values for CronetTrafficReported.request_body_size
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_UNSPECIFIED = 0;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_ZERO = 1;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_UNDER_TEN_KIB = 2;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_TEN_TO_FIFTY_KIB =
                    3;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_FIFTY_TO_TWO_HUNDRED_KIB =
                    4;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_TWO_HUNDRED_TO_FIVE_HUNDRED_KIB =
                    5;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_FIVE_HUNDRED_KIB_TO_ONE_MIB =
                    6;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_ONE_TO_FIVE_MIB =
                    7;
    public static final int
            CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_OVER_FIVE_MIB = 8;

    // Values for CronetTrafficReported.response_headers_size
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_UNSPECIFIED =
                    0;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_UNDER_ONE_KIB =
                    1;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_ONE_TO_TEN_KIB =
                    2;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_TEN_TO_TWENTY_FIVE_KIB =
                    3;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_TWENTY_FIVE_TO_FIFTY_KIB =
                    4;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_FIFTY_TO_HUNDRED_KIB =
                    5;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_OVER_HUNDRED_KIB =
                    6;

    // Values for CronetTrafficReported.response_body_size
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_UNSPECIFIED = 0;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_ZERO = 1;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_UNDER_TEN_KIB =
                    2;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_TEN_TO_FIFTY_KIB =
                    3;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_FIFTY_TO_TWO_HUNDRED_KIB =
                    4;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_TWO_HUNDRED_TO_FIVE_HUNDRED_KIB =
                    5;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_FIVE_HUNDRED_KIB_TO_ONE_MIB =
                    6;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_ONE_TO_FIVE_MIB =
                    7;
    public static final int
            CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_OVER_FIVE_MIB =
                    8;

    // Values for CronetTrafficReported.terminal_state
    public static final int CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_UNKNOWN = 0;
    public static final int CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_SUCCEEDED = 1;
    public static final int CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_ERROR = 2;
    public static final int CRONET_TRAFFIC_REPORTED__TERMINAL_STATE__STATE_CANCELLED = 3;

    // Values for CronetTrafficReported.is_bidi_stream
    public static final int CRONET_TRAFFIC_REPORTED__IS_BIDI_STREAM__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int CRONET_TRAFFIC_REPORTED__IS_BIDI_STREAM__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int CRONET_TRAFFIC_REPORTED__IS_BIDI_STREAM__OPTIONAL_BOOLEAN_FALSE = 2;

    // Values for CronetTrafficReported.final_user_callback_threw
    public static final int
            CRONET_TRAFFIC_REPORTED__FINAL_USER_CALLBACK_THREW__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int
            CRONET_TRAFFIC_REPORTED__FINAL_USER_CALLBACK_THREW__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int
            CRONET_TRAFFIC_REPORTED__FINAL_USER_CALLBACK_THREW__OPTIONAL_BOOLEAN_FALSE = 2;

    // Values for CronetTrafficReported.quic_connection_close_source
    public static final int
            CRONET_TRAFFIC_REPORTED__QUIC_CONNECTION_CLOSE_SOURCE__CONNECTION_CLOSE_UNKNOWN = 0;
    public static final int
            CRONET_TRAFFIC_REPORTED__QUIC_CONNECTION_CLOSE_SOURCE__CONNECTION_CLOSE_SELF = 1;
    public static final int
            CRONET_TRAFFIC_REPORTED__QUIC_CONNECTION_CLOSE_SOURCE__CONNECTION_CLOSE_PEER = 2;

    // Values for CronetTrafficReported.failure_reason
    public static final int CRONET_TRAFFIC_REPORTED__FAILURE_REASON__FAILURE_REASON_UNKNOWN = 0;
    public static final int CRONET_TRAFFIC_REPORTED__FAILURE_REASON__FAILURE_REASON_NETWORK = 1;
    public static final int CRONET_TRAFFIC_REPORTED__FAILURE_REASON__FAILURE_REASON_OTHER = 100;

    // Values for CronetTrafficReported.is_socket_reused
    public static final int CRONET_TRAFFIC_REPORTED__IS_SOCKET_REUSED__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int CRONET_TRAFFIC_REPORTED__IS_SOCKET_REUSED__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int CRONET_TRAFFIC_REPORTED__IS_SOCKET_REUSED__OPTIONAL_BOOLEAN_FALSE = 2;

    // Values for CronetEngineBuilderInitialized.author
    public static final int CRONET_ENGINE_BUILDER_INITIALIZED__AUTHOR__AUTHOR_UNSPECIFIED = 0;
    public static final int CRONET_ENGINE_BUILDER_INITIALIZED__AUTHOR__AUTHOR_API = 1;
    public static final int CRONET_ENGINE_BUILDER_INITIALIZED__AUTHOR__AUTHOR_IMPL = 2;

    // Values for CronetEngineBuilderInitialized.source
    public static final int CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_UNSPECIFIED =
            0;
    public static final int
            CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_EMBEDDED_NATIVE = 1;
    public static final int
            CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_GMSCORE_NATIVE = 2;
    public static final int CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_EMBEDDED_JAVA =
            3;
    public static final int
            CRONET_ENGINE_BUILDER_INITIALIZED__SOURCE__CRONET_SOURCE_HTTPENGINE_NATIVE = 4;

    // Values for CronetEngineBuilderInitialized.creation_successful
    public static final int
            CRONET_ENGINE_BUILDER_INITIALIZED__CREATION_SUCCESSFUL__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int
            CRONET_ENGINE_BUILDER_INITIALIZED__CREATION_SUCCESSFUL__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int
            CRONET_ENGINE_BUILDER_INITIALIZED__CREATION_SUCCESSFUL__OPTIONAL_BOOLEAN_FALSE = 2;

    // Values for CronetInitialized.http_flags_successful
    public static final int CRONET_INITIALIZED__HTTP_FLAGS_SUCCESSFUL__OPTIONAL_BOOLEAN_UNSET = 0;
    public static final int CRONET_INITIALIZED__HTTP_FLAGS_SUCCESSFUL__OPTIONAL_BOOLEAN_TRUE = 1;
    public static final int CRONET_INITIALIZED__HTTP_FLAGS_SUCCESSFUL__OPTIONAL_BOOLEAN_FALSE = 2;

    // Annotation constants.
    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_IS_UID = StatsLog.ANNOTATION_ID_IS_UID;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_TRUNCATE_TIMESTAMP =
            StatsLog.ANNOTATION_ID_TRUNCATE_TIMESTAMP;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_PRIMARY_FIELD = StatsLog.ANNOTATION_ID_PRIMARY_FIELD;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_EXCLUSIVE_STATE = StatsLog.ANNOTATION_ID_EXCLUSIVE_STATE;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_PRIMARY_FIELD_FIRST_UID =
            StatsLog.ANNOTATION_ID_PRIMARY_FIELD_FIRST_UID;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_DEFAULT_STATE = StatsLog.ANNOTATION_ID_DEFAULT_STATE;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_TRIGGER_STATE_RESET =
            StatsLog.ANNOTATION_ID_TRIGGER_STATE_RESET;

    @android.annotation.SuppressLint("InlinedApi")
    public static final byte ANNOTATION_ID_STATE_NESTED = StatsLog.ANNOTATION_ID_STATE_NESTED;

    // Write methods
    public static void write(
            int code,
            long arg1,
            int arg2,
            int arg3,
            int arg4,
            int arg5,
            int arg6,
            boolean arg7,
            boolean arg8,
            int arg9,
            boolean arg10,
            boolean arg11,
            boolean arg12,
            int arg13,
            java.lang.String arg14,
            int arg15,
            int arg16,
            int arg17,
            int arg18,
            int arg19,
            int arg20,
            int arg21,
            int arg22,
            int arg23,
            int arg24,
            int arg25,
            int arg26,
            int arg27,
            int arg28,
            int arg29,
            int arg30,
            int arg31,
            int arg32,
            int arg33,
            int arg34,
            int arg35,
            long arg36) {
        final StatsEvent.Builder builder = StatsEvent.newBuilder();
        builder.setAtomId(code);
        builder.writeLong(arg1);
        builder.writeInt(arg2);
        builder.writeInt(arg3);
        builder.writeInt(arg4);
        builder.writeInt(arg5);
        builder.writeInt(arg6);
        builder.writeBoolean(arg7);
        builder.writeBoolean(arg8);
        builder.writeInt(arg9);
        builder.writeBoolean(arg10);
        builder.writeBoolean(arg11);
        builder.writeBoolean(arg12);
        builder.writeInt(arg13);
        builder.writeString(arg14);
        builder.writeInt(arg15);
        builder.writeInt(arg16);
        builder.writeInt(arg17);
        builder.writeInt(arg18);
        builder.writeInt(arg19);
        builder.writeInt(arg20);
        builder.writeInt(arg21);
        builder.writeInt(arg22);
        builder.writeInt(arg23);
        builder.writeInt(arg24);
        builder.writeInt(arg25);
        builder.writeInt(arg26);
        builder.writeInt(arg27);
        builder.writeInt(arg28);
        builder.writeInt(arg29);
        builder.writeInt(arg30);
        builder.writeInt(arg31);
        builder.writeInt(arg32);
        builder.writeInt(arg33);
        builder.writeInt(arg34);
        builder.writeInt(arg35);
        builder.writeLong(arg36);

        builder.usePooledBuffer();
        StatsLog.write(builder.build());
    }

    public static void write(
            int code,
            long arg1,
            int arg2,
            int arg3,
            int arg4,
            int arg5,
            int arg6,
            int arg7,
            int arg8,
            int arg9,
            int arg10,
            int arg11,
            int arg12,
            int arg13,
            int arg14) {
        final StatsEvent.Builder builder = StatsEvent.newBuilder();
        builder.setAtomId(code);
        builder.writeLong(arg1);
        builder.writeInt(arg2);
        builder.writeInt(arg3);
        builder.writeInt(arg4);
        builder.writeInt(arg5);
        builder.writeInt(arg6);
        builder.writeInt(arg7);
        builder.writeInt(arg8);
        builder.writeInt(arg9);
        builder.writeInt(arg10);
        builder.writeInt(arg11);
        builder.writeInt(arg12);
        builder.writeInt(arg13);
        builder.writeInt(arg14);
        if (CRONET_ENGINE_BUILDER_INITIALIZED == code) {
            builder.addBooleanAnnotation(ANNOTATION_ID_IS_UID, true);
        }

        builder.usePooledBuffer();
        StatsLog.write(builder.build());
    }

    public static void write(
            int code,
            long arg1,
            int arg2,
            int arg3,
            int arg4,
            int arg5,
            int arg6,
            long arg7,
            int arg8,
            int arg9,
            boolean arg10,
            boolean arg11,
            int arg12,
            int arg13,
            int arg14,
            long arg15,
            long arg16,
            int arg17,
            int arg18,
            int arg19,
            int arg20,
            int arg21,
            int arg22,
            int arg23,
            int arg24,
            int arg25,
            int arg26) {
        final StatsEvent.Builder builder = StatsEvent.newBuilder();
        builder.setAtomId(code);
        builder.writeLong(arg1);
        builder.writeInt(arg2);
        builder.writeInt(arg3);
        builder.writeInt(arg4);
        builder.writeInt(arg5);
        builder.writeInt(arg6);
        builder.writeLong(arg7);
        builder.writeInt(arg8);
        builder.writeInt(arg9);
        builder.writeBoolean(arg10);
        builder.writeBoolean(arg11);
        builder.writeInt(arg12);
        builder.writeInt(arg13);
        builder.writeInt(arg14);
        builder.writeLong(arg15);
        builder.writeLong(arg16);
        builder.writeInt(arg17);
        builder.writeInt(arg18);
        builder.writeInt(arg19);
        builder.writeInt(arg20);
        builder.writeInt(arg21);
        if (CRONET_TRAFFIC_REPORTED == code) {
            builder.addBooleanAnnotation(ANNOTATION_ID_IS_UID, true);
        }
        builder.writeInt(arg22);
        builder.writeInt(arg23);
        builder.writeInt(arg24);
        builder.writeInt(arg25);
        builder.writeInt(arg26);

        builder.usePooledBuffer();
        StatsLog.write(builder.build());
    }

    @android.annotation.SuppressLint("ObsoleteSdkInt")
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    public static void write(
            int code, long arg1, int arg2, int arg3, int arg4, int arg5, long[] arg6, long[] arg7) {
        final StatsEvent.Builder builder = StatsEvent.newBuilder();
        builder.setAtomId(code);
        builder.writeLong(arg1);
        builder.writeInt(arg2);
        builder.writeInt(arg3);
        builder.writeInt(arg4);
        builder.writeInt(arg5);
        builder.writeLongArray(null == arg6 ? new long[0] : arg6);
        builder.writeLongArray(null == arg7 ? new long[0] : arg7);

        builder.usePooledBuffer();
        StatsLog.write(builder.build());
    }
}
