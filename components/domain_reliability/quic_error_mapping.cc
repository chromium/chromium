// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/domain_reliability/quic_error_mapping.h"

namespace domain_reliability {

namespace {

const struct QuicErrorMapping {
  quic::QuicErrorCode quic_error;
  const char* beacon_quic_error;
} kQuicErrorMap[] = {
    // Connection has reached an invalid state.
    {quic::QUIC_INTERNAL_ERROR, "quic.internal_error"},
    // There were data frames after the a fin or reset.
    {quic::QUIC_STREAM_DATA_AFTER_TERMINATION,
     "quic.stream_data.after_termination"},
    // Control frame is malformed.
    {quic::QUIC_INVALID_PACKET_HEADER, "quic.invalid.packet_header"},
    // Frame data is malformed.
    {quic::QUIC_INVALID_FRAME_DATA, "quic.invalid_frame_data"},
    // The packet contained no payload.
    {quic::QUIC_MISSING_PAYLOAD, "quic.missing.payload"},
    // FEC data is malformed.
    {quic::QUIC_INVALID_FEC_DATA, "quic.invalid.fec_data"},
    // STREAM frame data is malformed.
    {quic::QUIC_INVALID_STREAM_DATA, "quic.invalid.stream_data"},
    // STREAM frame data is not encrypted.
    {quic::QUIC_UNENCRYPTED_STREAM_DATA, "quic.unencrypted.stream_data"},
    // Attempt to send unencrypted STREAM frame.
    {quic::QUIC_ATTEMPT_TO_SEND_UNENCRYPTED_STREAM_DATA,
     "quic.attempt.to.unencrypted.stream.data"},
    // Received a frame which is likely the result of memory corruption.
    {quic::QUIC_MAYBE_CORRUPTED_MEMORY, "quic.maybe.corrupted.momery"},
    // FEC frame data is not encrypted.
    {quic::QUIC_UNENCRYPTED_FEC_DATA, "quic.unencrypted.fec.data"},
    // RST_STREAM frame data is malformed.
    {quic::QUIC_INVALID_RST_STREAM_DATA, "quic.invalid.rst_stream_data"},
    // CONNECTION_CLOSE frame data is malformed.
    {quic::QUIC_INVALID_CONNECTION_CLOSE_DATA,
     "quic.invalid.connection_close_data"},
    // GOAWAY frame data is malformed.
    {quic::QUIC_INVALID_GOAWAY_DATA, "quic.invalid.goaway_data"},
    // WINDOW_UPDATE frame data is malformed.
    {quic::QUIC_INVALID_WINDOW_UPDATE_DATA, "quic.invalid.window_update_data"},
    // BLOCKED frame data is malformed.
    {quic::QUIC_INVALID_BLOCKED_DATA, "quic.invalid.blocked_data"},
    // STOP_WAITING frame data is malformed.
    {quic::QUIC_INVALID_STOP_WAITING_DATA, "quic.invalid.stop_waiting_data"},
    // PATH_CLOSE frame data is malformed.
    {quic::QUIC_INVALID_PATH_CLOSE_DATA, "quic.invalid_path_close_data"},
    // ACK frame data is malformed.
    {quic::QUIC_INVALID_ACK_DATA, "quic.invalid.ack_data"},

    // Version negotiation packet is malformed.
    {quic::QUIC_INVALID_VERSION_NEGOTIATION_PACKET,
     "quic_invalid_version_negotiation_packet"},
    // Public RST packet is malformed.
    {quic::QUIC_INVALID_PUBLIC_RST_PACKET, "quic.invalid.public_rst_packet"},

    // There was an error decrypting.
    {quic::QUIC_DECRYPTION_FAILURE, "quic.decryption.failure"},
    // There was an error encrypting.
    {quic::QUIC_ENCRYPTION_FAILURE, "quic.encryption.failure"},
    // The packet exceeded kMaxPacketSize.
    {quic::QUIC_PACKET_TOO_LARGE, "quic.packet.too_large"},
    // The peer is going away.  May be a client or server.
    {quic::QUIC_PEER_GOING_AWAY, "quic.peer_going_away"},
    // A stream ID was invalid.
    {quic::QUIC_INVALID_STREAM_ID, "quic.invalid_stream_id"},
    // A priority was invalid.
    {quic::QUIC_INVALID_PRIORITY, "quic.invalid_priority"},
    // Too many streams already open.
    {quic::QUIC_TOO_MANY_OPEN_STREAMS, "quic.too_many_open_streams"},
    // The peer created too many available streams.
    {quic::QUIC_TOO_MANY_AVAILABLE_STREAMS, "quic.too_many_available_streams"},
    // Received public reset for this connection.
    {quic::QUIC_PUBLIC_RESET, "quic.public_reset"},
    // Version selected by client is not acceptable to the server.
    {quic::QUIC_INVALID_VERSION, "quic.invalid_version"},

    // The Header ID for a stream was too far from the previous.
    {quic::QUIC_INVALID_HEADER_ID, "quic.invalid_header_id"},
    // Negotiable parameter received during handshake had invalid value.
    {quic::QUIC_INVALID_NEGOTIATED_VALUE, "quic.invalid_negotiated_value"},
    // There was an error decompressing data.
    {quic::QUIC_DECOMPRESSION_FAILURE, "quic.decompression_failure"},
    // We hit our prenegotiated (or default) timeout
    {quic::QUIC_NETWORK_IDLE_TIMEOUT, "quic.connection.idle_time_out"},
    // We hit our overall connection timeout
    {quic::QUIC_HANDSHAKE_TIMEOUT, "quic.connection.handshake_timed_out"},
    // There was an error encountered migrating addresses.
    {quic::QUIC_ERROR_MIGRATING_ADDRESS, "quic.error_migrating_address"},
    // There was an error encountered migrating port only.
    {quic::QUIC_ERROR_MIGRATING_PORT, "quic.error_migrating_port"},
    // There was an error while writing to the socket.
    {quic::QUIC_PACKET_WRITE_ERROR, "quic.packet.write_error"},
    // There was an error while reading from the socket.
    {quic::QUIC_PACKET_READ_ERROR, "quic.packet.read_error"},
    // We received a STREAM_FRAME with no data and no fin flag set.
    {quic::QUIC_EMPTY_STREAM_FRAME_NO_FIN, "quic.empty_stream_frame_no_fin"},
    // We received invalid data on the headers stream.
    {quic::QUIC_INVALID_HEADERS_STREAM_DATA,
     "quic.invalid_headers_stream_data"},
    // The peer received too much data, violating flow control.
    {quic::QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA,
     "quic.flow_control.received_too_much_data"},
    // The peer sent too much data, violating flow control.
    {quic::QUIC_FLOW_CONTROL_SENT_TOO_MUCH_DATA,
     "quic.flow_control.sent_too_much_data"},
    // The peer received an invalid flow control window.
    {quic::QUIC_FLOW_CONTROL_INVALID_WINDOW,
     "quic.flow_control.invalid_window"},
    // The connection has been IP pooled into an existing connection.
    {quic::QUIC_CONNECTION_IP_POOLED, "quic.connection.ip_pooled"},
    // The connection has too many outstanding sent packets.
    {quic::QUIC_TOO_MANY_OUTSTANDING_SENT_PACKETS,
     "quic.too_many_outstanding_sent_packets"},
    // The connection has too many outstanding received packets.
    {quic::QUIC_TOO_MANY_OUTSTANDING_RECEIVED_PACKETS,
     "quic.too_many_outstanding_received_packets"},
    // The quic connection job to load server config is cancelled.
    {quic::QUIC_CONNECTION_CANCELLED, "quic.connection.cancelled"},
    // Disabled QUIC because of high packet loss rate.
    {quic::QUIC_BAD_PACKET_LOSS_RATE, "quic.bad_packet_loss_rate"},
    // Disabled QUIC because of too many PUBLIC_RESETs post handshake.
    {quic::QUIC_PUBLIC_RESETS_POST_HANDSHAKE,
     "quic.public_resets_post_handshake"},
    // Closed because we failed to serialize a packet.
    {quic::QUIC_FAILED_TO_SERIALIZE_PACKET, "quic.failed_to_serialize_packet"},
    // QUIC timed out after too many RTOs.
    {quic::QUIC_TOO_MANY_RTOS, "quic.too_many_rtos"},
    // Crypto errors.

    // Hanshake failed.
    {quic::QUIC_HANDSHAKE_FAILED, "quic.handshake_failed"},
    // Handshake message contained out of order tags.
    {quic::QUIC_CRYPTO_TAGS_OUT_OF_ORDER, "quic.crypto.tags_out_of_order"},
    // Handshake message contained too many entries.
    {quic::QUIC_CRYPTO_TOO_MANY_ENTRIES, "quic.crypto.too_many_entries"},
    // Handshake message contained an invalid value length.
    {quic::QUIC_CRYPTO_INVALID_VALUE_LENGTH,
     "quic.crypto.invalid_value_length"},
    // A crypto message was received after the handshake was complete.
    {quic::QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
     "quic.crypto_message_after_handshake_complete"},
    // A crypto message was received with an illegal message tag.
    {quic::QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
     "quic.invalid_crypto_message_type"},
    // A crypto message was received with an illegal parameter.
    {quic::QUIC_INVALID_CRYPTO_MESSAGE_PARAMETER,
     "quic.invalid_crypto_message_parameter"},
    // An invalid channel id signature was supplied.
    {quic::QUIC_INVALID_CHANNEL_ID_SIGNATURE,
     "quic.invalid_channel_id_signature"},
    // A crypto message was received with a mandatory parameter missing.
    {quic::QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND,
     "quic.crypto_message.parameter_not_found"},
    // A crypto message was received with a parameter that has no overlap
    // with the local parameter.
    {quic::QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP,
     "quic.crypto_message.parameter_no_overlap"},
    // A crypto message was received that contained a parameter with too few
    // values.
    {quic::QUIC_CRYPTO_MESSAGE_INDEX_NOT_FOUND,
     "quic_crypto_message_index_not_found"},
    // A demand for an unsupport proof type was received.
    {quic::QUIC_UNSUPPORTED_PROOF_DEMAND, "quic.unsupported_proof_demand"},
    // An internal error occured in crypto processing.
    {quic::QUIC_CRYPTO_INTERNAL_ERROR, "quic.crypto.internal_error"},
    // A crypto handshake message specified an unsupported version.
    {quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED,
     "quic.crypto.version_not_supported"},
    // QUIC_CRYPTO_STATELESS_REJECT was code 72. The code has been
    // deprecated, but to keep the assert below happy, there needs to be
    // an entry for it, but the symbol is gone.
    {static_cast<quic::QuicErrorCode>(72),
     "quic.crypto.handshake_stateless_reject"},
    // There was no intersection between the crypto primitives supported by the
    // peer and ourselves.
    {quic::QUIC_CRYPTO_NO_SUPPORT, "quic.crypto.no_support"},
    // The server rejected our client hello messages too many times.
    {quic::QUIC_CRYPTO_TOO_MANY_REJECTS, "quic.crypto.too_many_rejects"},
    // The client rejected the server's certificate chain or signature.
    {quic::QUIC_PROOF_INVALID, "quic.proof_invalid"},
    // A crypto message was received with a duplicate tag.
    {quic::QUIC_CRYPTO_DUPLICATE_TAG, "quic.crypto.duplicate_tag"},
    // A crypto message was received with the wrong encryption level (i.e. it
    // should have been encrypted but was not.)
    {quic::QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
     "quic.crypto.encryption_level_incorrect"},
    // The server config for a server has expired.
    {quic::QUIC_CRYPTO_SERVER_CONFIG_EXPIRED,
     "quic.crypto.server_config_expired"},
    // We failed to setup the symmetric keys for a connection.
    {quic::QUIC_CRYPTO_SYMMETRIC_KEY_SETUP_FAILED,
     "quic.crypto.symmetric_key_setup_failed"},
    // A handshake message arrived, but we are still validating the
    // previous handshake message.
    {quic::QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO,
     "quic.crypto_message_while_validating_client_hello"},
    // A server config update arrived before the handshake is complete.
    {quic::QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE,
     "quic.crypto.update_before_handshake_complete"},
    // CHLO cannot fit in one packet.
    {quic::QUIC_CRYPTO_CHLO_TOO_LARGE, "quic.crypto.chlo_too_large"},
    // This connection involved a version negotiation which appears to have been
    // tampered with.
    {quic::QUIC_VERSION_NEGOTIATION_MISMATCH,
     "quic.version_negotiation_mismatch"},

    // Multipath is not enabled, but a packet with multipath flag on is
    // received.
    {quic::QUIC_BAD_MULTIPATH_FLAG, "quic.bad_multipath_flag"},
    // A path is supposed to exist but does not.
    {quic::QUIC_MULTIPATH_PATH_DOES_NOT_EXIST,
     "quic.quic_multipath_path_does_not_exist"},
    // A path is supposed to be active but is not.
    {quic::QUIC_MULTIPATH_PATH_NOT_ACTIVE,
     "quic.quic_multipath_path_not_active"},

    // Network change and connection migration errors.

    // IP address changed causing connection close.
    {quic::QUIC_IP_ADDRESS_CHANGED, "quic.ip_address_changed"},
    // Network changed, but connection had no migratable streams.
    {quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
     "quic.connection_migration_no_migratable_streams"},
    // Connection changed networks too many times.
    {quic::QUIC_CONNECTION_MIGRATION_TOO_MANY_CHANGES,
     "quic.connection_migration_too_many_changes"},
    // Connection migration was attempted, but there was no new network to
    // migrate to.
    {quic::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK,
     "quic.connection_migration_no_new_network"},
    // Network changed, but connection had one or more non-migratable streams.
    {quic::QUIC_CONNECTION_MIGRATION_NON_MIGRATABLE_STREAM,
     "quic.connection_migration_non_migratable_stream"},
    // Network changed, but connection migration was disabled by config.
    {quic::QUIC_CONNECTION_MIGRATION_DISABLED_BY_CONFIG,
     "quic.connection_migration_disabled_by_config"},
    // Network changed, but error was encountered on the alternative network.
    {quic::QUIC_CONNECTION_MIGRATION_INTERNAL_ERROR,
     "quic.connection_migration_internal_error"},
    // Network changed, but error was encountered on the alternative network.
    {quic::QUIC_CONNECTION_MIGRATION_HANDSHAKE_UNCONFIRMED,
     "quic.connection_migration_handshake_unconfirmed"},

    // Stream frame overlaps with buffered data.
    {quic::QUIC_OVERLAPPING_STREAM_DATA, "quic.overlapping_stream_data"},
    // Stream frames arrived too discontiguously so that stream sequencer buffer
    // maintains too many intervals.
    {quic::QUIC_TOO_MANY_STREAM_DATA_INTERVALS,
     "quic.too_many_stream_data_intervals"},
    // Sequencer buffer get into weird state where continuing read/write
    // will lead to crash.
    {quic::QUIC_STREAM_SEQUENCER_INVALID_STATE,
     "quic.stream_sequencer_invalid_state"},
    // Connection closed because of server hits max number of sessions allowed.
    {quic::QUIC_TOO_MANY_SESSIONS_ON_SERVER,
     "quic.too_many_sessions_on_server"},
    // There was an error decompressing data.
    {quic::QUIC_DECOMPRESSION_FAILURE, "quic.decompression_failure"},
    // Receive a RST_STREAM with offset larger than kMaxStreamLength.
    {quic::QUIC_STREAM_LENGTH_OVERFLOW, "quic.stream_length_overflow"},
    // Received a MAX DATA frame with errors.
    {quic::QUIC_INVALID_MAX_DATA_FRAME_DATA,
     "quic.invalid.max_data_frame_data"},
    // Received a MAX STREAM DATA frame with errors.
    {quic::QUIC_INVALID_MAX_STREAM_DATA_FRAME_DATA,
     "quic.invalid.max_stream_data_frame_data"},
    // Received a MAX STREAMS frame with bad data
    {quic::QUIC_MAX_STREAMS_DATA, "quic.max_streams_data"},
    // Received a STREAMS BLOCKED frame with bad data
    {quic::QUIC_STREAMS_BLOCKED_DATA, "quic.streams_blocked_data"},
    // Error deframing a STREAM BLOCKED frame.
    {quic::QUIC_INVALID_STREAM_BLOCKED_DATA,
     "quic.invalid.stream_blocked_data"},
    // NEW CONNECTION ID frame data is malformed.
    {quic::QUIC_INVALID_NEW_CONNECTION_ID_DATA,
     "quic.invalid.new_connection_id_data"},
    // Received a MAX STREAM DATA frame with errors.
    {quic::QUIC_INVALID_STOP_SENDING_FRAME_DATA,
     "quic.invalid.stop_sending_frame_data"},
    // Error deframing PATH CHALLENGE or PATH RESPONSE frames.
    {quic::QUIC_INVALID_PATH_CHALLENGE_DATA,
     "quic.invalid.path_challenge_data"},
    {quic::QUIC_INVALID_PATH_RESPONSE_DATA, "quic.invalid.path_response_data"},
    {quic::QUIC_INVALID_MESSAGE_DATA, "quic.invalid.message_data"},
    {quic::IETF_QUIC_PROTOCOL_VIOLATION, "quic.ietf.protocol_violation"},
    {quic::QUIC_INVALID_NEW_TOKEN, "quic.invalid_new_token"},
    {quic::QUIC_DATA_RECEIVED_ON_WRITE_UNIDIRECTIONAL_STREAM,
     "quic.data.received.on.write.unidirectional.stream"},
    {quic::QUIC_TRY_TO_WRITE_DATA_ON_READ_UNIDIRECTIONAL_STREAM,
     "quic.try.to.write.data.on.read.unidirectional.stream"},
    {quic::QUIC_INVALID_RETIRE_CONNECTION_ID_DATA,
     "quic.invalid.retire.connection.id.data"},
    {quic::QUIC_STREAMS_BLOCKED_ERROR,
     "quic.stream.id.in.streams_blocked.frame"},
    {quic::QUIC_MAX_STREAMS_ERROR, "quic.stream.id.in.max_streams.frame"},
    {quic::QUIC_HTTP_DECODER_ERROR, "quic.http.decoder.error"},
    {quic::QUIC_STALE_CONNECTION_CANCELLED, "quic.stale.connection.cancelled"},
    {quic::QUIC_IETF_GQUIC_ERROR_MISSING, "quic.ietf.gquic.error_missing"},
    {quic::QUIC_WINDOW_UPDATE_RECEIVED_ON_READ_UNIDIRECTIONAL_STREAM,
     "quic.window.update.received.on.read.unidirectional.stream"},
    {quic::QUIC_TOO_MANY_BUFFERED_CONTROL_FRAMES,
     "quic.too.many.buffered.control.frames"},
    {quic::QUIC_TRANSPORT_INVALID_CLIENT_INDICATION,
     "quic.transport.invalid.client.indication"},

    {quic::QUIC_QPACK_DECOMPRESSION_FAILED, "quic.qpack.decompression.failed"},
    {quic::QUIC_QPACK_ENCODER_STREAM_ERROR, "quic.qpack.encoder.stream.error"},
    {quic::QUIC_QPACK_DECODER_STREAM_ERROR, "quic.qpack.decoder.stream.error"},

    {quic::QUIC_QPACK_ENCODER_STREAM_INTEGER_TOO_LARGE,
     "quic.qpack.encoder.stream.integer.too.large"},
    {quic::QUIC_QPACK_ENCODER_STREAM_STRING_LITERAL_TOO_LONG,
     "quic.qpack.encoder.stream.string.literal.too.long"},
    {quic::QUIC_QPACK_ENCODER_STREAM_HUFFMAN_ENCODING_ERROR,
     "quic.qpack.encoder.stream.huffman.encoding.error"},
    {quic::QUIC_QPACK_ENCODER_STREAM_INVALID_STATIC_ENTRY,
     "quic.qpack.encoder.stream.invalid.static.entry"},
    {quic::QUIC_QPACK_ENCODER_STREAM_ERROR_INSERTING_STATIC,
     "quic.qpack.encoder.stream.error.inserting.static"},
    {quic::QUIC_QPACK_ENCODER_STREAM_INSERTION_INVALID_RELATIVE_INDEX,
     "quic.qpack.encoder.stream.insertion.invalid.relative.index"},
    {quic::QUIC_QPACK_ENCODER_STREAM_INSERTION_DYNAMIC_ENTRY_NOT_FOUND,
     "quic.qpack.encoder.stream.insertion.dynamic.entry.not.found"},
    {quic::QUIC_QPACK_ENCODER_STREAM_ERROR_INSERTING_DYNAMIC,
     "quic.qpack.encoder.stream.error.inserting.dynamic"},
    {quic::QUIC_QPACK_ENCODER_STREAM_ERROR_INSERTING_LITERAL,
     "quic.qpack.encoder.stream.error.inserting.literal"},
    {quic::QUIC_QPACK_ENCODER_STREAM_DUPLICATE_INVALID_RELATIVE_INDEX,
     "quic.qpack.encoder.stream.duplicate.invalid.relative.index"},
    {quic::QUIC_QPACK_ENCODER_STREAM_DUPLICATE_DYNAMIC_ENTRY_NOT_FOUND,
     "quic.qpack.encoder.stream.duplicate.dynamic.entry.not.found"},
    {quic::QUIC_QPACK_ENCODER_STREAM_SET_DYNAMIC_TABLE_CAPACITY,
     "quic.qpack.encoder.stream.set.dynamic.table.capacity"},
    {quic::QUIC_QPACK_DECODER_STREAM_INTEGER_TOO_LARGE,
     "quic.qpack.decoder.stream.integer.too.large"},
    {quic::QUIC_QPACK_DECODER_STREAM_INVALID_ZERO_INCREMENT,
     "quic.qpack.decoder.stream.invalid.zero.increment"},
    {quic::QUIC_QPACK_DECODER_STREAM_INCREMENT_OVERFLOW,
     "quic.qpack.decoder.stream.increment.overflow"},
    {quic::QUIC_QPACK_DECODER_STREAM_IMPOSSIBLE_INSERT_COUNT,
     "quic.qpack.decoder.stream.impossible.insert.count"},
    {quic::QUIC_QPACK_DECODER_STREAM_INCORRECT_ACKNOWLEDGEMENT,
     "quic.qpack.decoder.stream.incorrect.acknowledgement"},

    {quic::QUIC_STREAM_DATA_BEYOND_CLOSE_OFFSET,
     "quic.stream.data.beyond.close.offset"},
    {quic::QUIC_STREAM_MULTIPLE_OFFSET, "quic.stream.multiple.offset"},

    {quic::QUIC_HTTP_FRAME_TOO_LARGE, "quic.http.frame.too.large,"},
    {quic::QUIC_HTTP_FRAME_ERROR, "quic.http.frame.error"},
    {quic::QUIC_HTTP_FRAME_UNEXPECTED_ON_SPDY_STREAM,
     "quic.http.frame.unexpected.on.spdy.stream"},
    {quic::QUIC_HTTP_FRAME_UNEXPECTED_ON_CONTROL_STREAM,
     "quic.http.frame.unexpected.on.control.stream"},

    {quic::QUIC_HPACK_INDEX_VARINT_ERROR, "quic.hpack.index_varint_error"},
    {quic::QUIC_HPACK_NAME_LENGTH_VARINT_ERROR,
     "quic.hpack.name_length_varint_error"},
    {quic::QUIC_HPACK_VALUE_LENGTH_VARINT_ERROR,
     "quic.hpack.value_length_varint_error"},
    {quic::QUIC_HPACK_NAME_TOO_LONG, "quic.hpack.name_too_long"},
    {quic::QUIC_HPACK_VALUE_TOO_LONG, "quic.hpack.value_too_long"},
    {quic::QUIC_HPACK_NAME_HUFFMAN_ERROR, "quic.hpack.name_huffman_error"},
    {quic::QUIC_HPACK_VALUE_HUFFMAN_ERROR, "quic.hpack.value_huffman_error"},
    {quic::QUIC_HPACK_MISSING_DYNAMIC_TABLE_SIZE_UPDATE,
     "quic.hpack.missing_dynamic_table_size_update"},
    {quic::QUIC_HPACK_INVALID_INDEX, "quic.hpack.invalid_index"},
    {quic::QUIC_HPACK_INVALID_NAME_INDEX, "quic.hpack.invalid_name_index"},
    {quic::QUIC_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_NOT_ALLOWED,
     "quic.hpack.dynamic_table_size_update_not_allowed"},
    {quic::QUIC_HPACK_INITIAL_TABLE_SIZE_UPDATE_IS_ABOVE_LOW_WATER_MARK,
     "quic.hpack.initial_table_size_update_is_above_low_water_mark"},
    {quic::QUIC_HPACK_TABLE_SIZE_UPDATE_IS_ABOVE_ACKNOWLEDGED_SETTING,
     "quic.hpack.table_size_update_is_above_acknowledge_setting"},
    {quic::QUIC_HPACK_TRUNCATED_BLOCK, "quic.hpack.truncated_block"},
    {quic::QUIC_HPACK_FRAGMENT_TOO_LONG, "quic.hpack.fragment_too_long"},
    {quic::QUIC_HPACK_COMPRESSED_HEADER_SIZE_EXCEEDS_LIMIT,
     "quic.hpack.compressed_header_size_exceeds_limit"},
    {quic::QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_SPDY_STREAM,
     "quic.http_invalid_frame_sequence_on_spdy_stream"},
    {quic::QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_CONTROL_STREAM,
     "quic.http_invalid_frame_sequence_on_control_stream"},
    {quic::QUIC_HTTP_DUPLICATE_UNIDIRECTIONAL_STREAM,
     "quic.http_duplicate_unidirectional_stream"},
    {quic::QUIC_HTTP_SERVER_INITIATED_BIDIRECTIONAL_STREAM,
     "quic.http_server_initiated_bidirectional_stream"},
    {quic::QUIC_HTTP_STREAM_WRONG_DIRECTION,
     "quic.http_stream_wrong_direction"},
    {quic::QUIC_HTTP_CLOSED_CRITICAL_STREAM,
     "quic.http_closed_critical_stream"},
    {quic::QUIC_HTTP_MISSING_SETTINGS_FRAME,
     "quic.http_missing_settings_frame"},
    {quic::QUIC_HTTP_DUPLICATE_SETTING_IDENTIFIER,
     "quic.http_duplicate_setting_identifier"},
    {quic::QUIC_HTTP_INVALID_MAX_PUSH_ID, "quic.http_invalid_max_push_id"},
    {quic::QUIC_HTTP_STREAM_LIMIT_TOO_LOW, "quic.http_stream_limit_too_low"},
    {quic::QUIC_ZERO_RTT_UNRETRANSMITTABLE, "quic.zero_rtt_unretransmittable"},
    {quic::QUIC_ZERO_RTT_REJECTION_LIMIT_REDUCED,
     "quic.zero_rtt_rejection_limit_reduced"},
    {quic::QUIC_ZERO_RTT_RESUMPTION_LIMIT_REDUCED,
     "quic.zero_rtt_resumption_limit_reduced"},
    {quic::QUIC_HTTP_ZERO_RTT_RESUMPTION_SETTINGS_MISMATCH,
     "quic.http_zero_rtt_resumption_settings_mismatch"},
    {quic::QUIC_HTTP_ZERO_RTT_REJECTION_SETTINGS_MISMATCH,
     "quic.http_zero_rtt_rejection_settings_mismatch"},
    {quic::QUIC_HTTP_GOAWAY_INVALID_STREAM_ID,
     "quic.quic_http_goaway_invalid_stream_id"},
    {quic::QUIC_HTTP_GOAWAY_ID_LARGER_THAN_PREVIOUS,
     "quic.quic_http_goaway_id_larger_than_previous"},
    {quic::QUIC_SILENT_IDLE_TIMEOUT, "quic.silent_idle_timeout"},
    {quic::QUIC_HTTP_RECEIVE_SPDY_SETTING, "quic.http_receive_spdy_setting"},
    {quic::QUIC_MISSING_WRITE_KEYS, "quic.missing_write_keys"},
    {quic::QUIC_HTTP_RECEIVE_SPDY_FRAME, "quic.http_receive_spdy_frame"},
    {quic::QUIC_HTTP_RECEIVE_SERVER_PUSH, "quic.http_receive_server_push"},
    {quic::QUIC_HTTP_INVALID_SETTING_VALUE,
     "quic::quic_http_invalid_setting_value"},

    {quic::QUIC_KEY_UPDATE_ERROR, "quic.quic_key_update_error"},
    {quic::QUIC_AEAD_LIMIT_REACHED, "quic.quic_aead_limit_reached"},

    // QUIC_INVALID_APPLICATION_CLOSE_DATA was code 101. The code has been
    // deprecated, but to keep the assert below happy, there needs to be
    // an entry for it, but the symbol is gone.
    {static_cast<quic::QuicErrorCode>(101),
     "quic.invalid.application_close_data"},

    {quic::QUIC_MAX_AGE_TIMEOUT, "quic.quic_max_age_timeout"},
    {quic::QUIC_INVALID_0RTT_PACKET_NUMBER_OUT_OF_ORDER,
     "quic.quic_invalid_0rtt_packet_number_out_of_order"},
    {quic::QUIC_INVALID_PRIORITY_UPDATE, "quic::quic_invalid_priority_update"},
    {quic::QUIC_PEER_PORT_CHANGE_HANDSHAKE_UNCONFIRMED,
     "quic.peer_port_change_handshake_unconfirmed"},

    {quic::QUIC_TLS_BAD_CERTIFICATE, "quic::quic_tls_bad_certificate"},
    {quic::QUIC_TLS_UNSUPPORTED_CERTIFICATE,
     "quic::quic_tls_unsupported_certificate"},
    {quic::QUIC_TLS_CERTIFICATE_REVOKED, "quic::quic_tls_certificate_revoked"},
    {quic::QUIC_TLS_CERTIFICATE_EXPIRED, "quic::quic_tls_certificate_expired"},
    {quic::QUIC_TLS_CERTIFICATE_UNKNOWN, "quic::quic_tls_certificate_unknown"},
    {quic::QUIC_TLS_INTERNAL_ERROR, "quic::quic_tls_internal_error"},
    {quic::QUIC_TLS_UNRECOGNIZED_NAME, "quic::quic_tls_unrecognized_name"},
    {quic::QUIC_TLS_CERTIFICATE_REQUIRED,
     "quic::quic_tls_certificate_required"},
    {quic::QUIC_CONNECTION_ID_LIMIT_ERROR,
     "quic::quic_connection_id_limit_error"},
    {quic::QUIC_TOO_MANY_CONNECTION_ID_WAITING_TO_RETIRE,
     "quic::quic_too_many_connection_id_waiting_to_retire"},
    {quic::QUIC_INVALID_CHARACTER_IN_FIELD_VALUE,
     "quic::quic_invalid_character_in_field_value"},

    {quic::QUIC_TLS_UNEXPECTED_KEYING_MATERIAL_EXPORT_LABEL,
     "quic::quic_tls_unexpected_keying_material_export_label"},
    {quic::QUIC_TLS_KEYING_MATERIAL_EXPORTS_MISMATCH,
     "quic::quic_tls_keying_material_exports_mismatch"},
    {quic::QUIC_TLS_KEYING_MATERIAL_EXPORT_NOT_AVAILABLE,
     "quic::quic_tls_keying_material_export_not_available"},
    {quic::QUIC_UNEXPECTED_DATA_BEFORE_ENCRYPTION_ESTABLISHED,
     "quic::quic_unexpected_data_before_encryption_established"},

    // Received packet indicates version that does not match connection version.
    {quic::QUIC_PACKET_WRONG_VERSION, "quic.packet_wrong_version"},

    // Error code related to backend health-check.
    {quic::QUIC_SERVER_UNHEALTHY, "quic.quic_server_unhealthy"},

    // Error code related to handshake failure due to packets buffered for too
    // long.
    {quic::QUIC_HANDSHAKE_FAILED_PACKETS_BUFFERED_TOO_LONG,
     "quic.quic_handshake_failed_packets_buffered_too_long"},

    // Handshake failed due to invalid hostname in ClientHello. Only sent from
    // server.
    {quic::QUIC_HANDSHAKE_FAILED_INVALID_HOSTNAME,
     "quic.quic_handshake_failed_invalid_hostname"},

    // Client application lost network access.
    {quic::QUIC_CLIENT_LOST_NETWORK_ACCESS,
     "quic.quic_client_lost_network_access"},

    // No error. Used as bound while iterating.
    {quic::QUIC_LAST_ERROR, "quic.last_error"}};

// Must be updated any time a quic::QuicErrorCode is deprecated in
// net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h.
const int kDeprecatedQuicErrorCount = 5;
const int kActiveQuicErrorCount =
    quic::QUIC_LAST_ERROR - kDeprecatedQuicErrorCount;

static_assert(std::size(kQuicErrorMap) == kActiveQuicErrorCount,
              "quic_error_map is not in sync with quic protocol!");

}  // namespace

// static
bool GetDomainReliabilityBeaconQuicError(quic::QuicErrorCode quic_error,
                                         std::string* beacon_quic_error_out) {
  if (quic_error != quic::QUIC_NO_ERROR) {
    // Convert a QUIC error.
    // TODO(juliatuttle): Consider sorting and using binary search?
    for (size_t i = 0; i < std::size(kQuicErrorMap); i++) {
      if (kQuicErrorMap[i].quic_error == quic_error) {
        *beacon_quic_error_out = kQuicErrorMap[i].beacon_quic_error;
        return true;
      }
    }
  }
  beacon_quic_error_out->clear();
  return false;
}

}  // namespace domain_reliability
