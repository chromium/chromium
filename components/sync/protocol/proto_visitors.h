// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_VISITORS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_VISITORS_H_

#include "components/sync/base/data_type.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/autofill_offer_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/autofill_wallet_credential_specifics.pb.h"
#include "components/sync/protocol/autofill_wallet_usage_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/deletion_origin.pb.h"
#include "components/sync/protocol/dictionary_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/note_entity.pb.h"
#include "components/sync/protocol/os_preference_specifics.pb.h"
#include "components/sync/protocol/os_priority_preference_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/persisted_entity_data.pb.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"
#include "components/sync/protocol/plus_address_specifics.pb.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/printer_specifics.pb.h"
#include "components/sync/protocol/printers_authorization_server_specifics.pb.h"
#include "components/sync/protocol/priority_preference_specifics.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/sharing_message_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/protocol/tab_group_attribution_metadata.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"

// This file implements VisitProtoFields() functions for sync protos.
//
// VisitProtoFields(visitor, proto) calls |visitor| for each field in
// |proto|. When called, |visitor| gets passed |proto|, field name and
// field value.
//
// VisitProtoFields() used to implement two distinctive features:
// 1. Serialization into base::Value::Dict
// 2. Proto memory usage estimation
//
// To achieve that it's very important for VisitProtoFields() to be free
// of any logic. It must just call visitor for each field in a proto.
//
// Logic (like clobbering sensitive fields) must be implemented in visitors.
// For example see how ToValueVisitor (from proto_value_conversions.cc)
// implements various customizations.

#define VISIT_(Kind, field) \
  if (proto.has_##field())  \
  visitor.Visit##Kind(proto, #field, proto.field())

// Generic version, calls visitor.Visit(). Handles almost everything except
// for special cases below.
#define VISIT(field) VISIT_(, field)

// 'bytes' protobuf type maps to std::string, and is indistinguishable
// from 'string' type. To solve that 'bytes' fields are special cased to
// call visitor.VisitBytes().
#define VISIT_BYTES(field) VISIT_(Bytes, field)

// We could use template magic (std::is_enum) to handle enums, but that would
// complicate visitors, and besides we already have special case for 'bytes',
// so just add one more special case. Calls visitor.VisitEnum().
#define VISIT_ENUM(field) VISIT_(Enum, field)

// Repeated fields are always present, so there are no 'has_<field>' methods.
// This macro unconditionally calls visitor.Visit().
#define VISIT_REP(field) visitor.Visit(proto, #field, proto.field());

// Repeated fields are always present, so there are no 'has_<field>' methods.
// This macro unconditionally calls visitor.VisitBytes().
#define VISIT_REP_BYTES(field) visitor.VisitBytes(proto, #field, proto.field());

// Values that are secrets do not have their contents reported in debugging
// output, only their lengths, but are still counted for things like memory
// estimation.
#define VISIT_SECRET(field) VISIT_(Secret, field)

#define VISIT_PROTO_FIELDS(proto) \
  template <class V>              \
  void VisitProtoFields(V& visitor, proto)

namespace syncer {

VISIT_PROTO_FIELDS(const sync_pb::AppListSpecifics& proto) {
  VISIT(item_id);
  VISIT_ENUM(item_type);
  VISIT(item_name);
  VISIT(parent_id);
  VISIT(item_ordinal);
  VISIT(item_pin_ordinal);
  VISIT(is_user_pinned);
  VISIT(promise_package_id);
}

VISIT_PROTO_FIELDS(const sync_pb::AppNotificationSettings& proto) {
  VISIT(initial_setup_done);
  VISIT(disabled);
  VISIT(oauth_client_id);
}

VISIT_PROTO_FIELDS(const sync_pb::AppSettingSpecifics& proto) {
  VISIT(extension_setting);
}

VISIT_PROTO_FIELDS(const sync_pb::AppSpecifics& proto) {
  VISIT(extension);
  VISIT(notification_settings);
  VISIT(app_launch_ordinal);
  VISIT(page_ordinal);
  VISIT_ENUM(launch_type);
  VISIT_REP(linked_app_icons);
  VISIT(bookmark_app_url);
  VISIT(bookmark_app_description);
  VISIT(bookmark_app_icon_color);
  VISIT(bookmark_app_scope);
  VISIT(bookmark_app_theme_color);
}

VISIT_PROTO_FIELDS(const sync_pb::ArcPackageSpecifics& proto) {
  VISIT(package_name);
  VISIT(package_version);
  VISIT(last_backup_android_id);
  VISIT(last_backup_time);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillOfferSpecifics& proto) {
  VISIT(id);
  VISIT(offer_details_url);
  VISIT_REP(merchant_domain);
  VISIT_REP(merchant_app_package);
  VISIT(offer_expiry_date);
  VISIT(card_linked_offer_data);
  VISIT(promo_code_offer_data);
  VISIT(display_strings);
  VISIT(percentage_reward);
  VISIT(fixed_amount_reward);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AutofillOfferSpecifics::CardLinkedOfferData& proto) {
  VISIT_REP(instrument_id);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AutofillOfferSpecifics::PromoCodeOfferData& proto) {
  VISIT(promo_code);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AutofillOfferSpecifics::DisplayStrings& proto) {
  VISIT(value_prop_text);
  VISIT(see_details_text_mobile);
  VISIT(see_details_text_desktop);
  VISIT(usage_instructions_text_mobile);
  VISIT(usage_instructions_text_desktop);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AutofillOfferSpecifics::PercentageReward& proto) {
  VISIT(percentage);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AutofillOfferSpecifics::FixedAmountReward& proto) {
  VISIT(amount);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillProfileSpecifics& proto) {
  VISIT(guid);
  VISIT(deprecated_origin);
  VISIT(use_count);
  VISIT(use_date);
  VISIT(profile_label);
  VISIT_REP(name_first);
  VISIT_REP(name_middle);
  VISIT_REP(name_last_first);
  VISIT_REP(name_last_conjunction);
  VISIT_REP(name_last_second);
  VISIT_REP(name_last);
  VISIT_REP(name_full);

  VISIT_REP(name_first_status);
  VISIT_REP(name_middle_status);
  VISIT_REP(name_last_first_status);
  VISIT_REP(name_last_conjunction_status);
  VISIT_REP(name_last_second_status);
  VISIT_REP(name_last_status);
  VISIT_REP(name_full_status);

  VISIT_REP(email_address);
  VISIT(company_name);

  VISIT(address_home_line1);
  VISIT(address_home_line2);
  VISIT(address_home_city);
  VISIT(address_home_state);
  VISIT(address_home_zip);
  VISIT(address_home_country);
  VISIT(address_home_landmark);
  VISIT(address_home_overflow);
  VISIT(address_home_between_streets);
  VISIT(address_home_between_streets_1);
  VISIT(address_home_between_streets_2);
  VISIT(address_home_between_streets_or_landmark);
  VISIT(address_home_overflow_and_landmark);
  VISIT(address_home_admin_level_2);
  VISIT(address_home_street_address);
  VISIT(address_home_sorting_code);
  VISIT(address_home_dependent_locality);
  VISIT(address_home_thoroughfare_name);
  VISIT(address_home_thoroughfare_number);
  VISIT(address_home_subpremise_name);
  VISIT(address_home_apt);
  VISIT(address_home_apt_num);
  VISIT(address_home_apt_type);
  VISIT(address_home_street_location_and_locality);
  VISIT(address_home_thoroughfare_number_and_apt);

  VISIT_ENUM(address_home_city_status);
  VISIT_ENUM(address_home_state_status);
  VISIT_ENUM(address_home_zip_status);
  VISIT_ENUM(address_home_country_status);
  VISIT_ENUM(address_home_landmark_status);
  VISIT_ENUM(address_home_overflow_status);
  VISIT_ENUM(address_home_between_streets_status);
  VISIT_ENUM(address_home_between_streets_1_status);
  VISIT_ENUM(address_home_between_streets_2_status);
  VISIT_ENUM(address_home_between_streets_or_landmark_status);
  VISIT_ENUM(address_home_overflow_and_landmark_status);
  VISIT_ENUM(address_home_admin_level_2_status);
  VISIT_ENUM(address_home_street_address_status);
  VISIT_ENUM(address_home_sorting_code_status);
  VISIT_ENUM(address_home_dependent_locality_status);
  VISIT_ENUM(address_home_thoroughfare_name_status);
  VISIT_ENUM(address_home_thoroughfare_number_status);
  VISIT_ENUM(address_home_subpremise_name_status);
  VISIT_ENUM(address_home_apt_status);
  VISIT_ENUM(address_home_apt_num_status);
  VISIT_ENUM(address_home_apt_type_status);
  VISIT_ENUM(address_home_street_location_and_locality_status);
  VISIT_ENUM(address_home_thoroughfare_number_and_apt_status);

  VISIT(address_home_language_code);
  VISIT_REP(phone_home_whole_number);
  VISIT(validity_state_bitfield);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillSpecifics& proto) {
  VISIT(name);
  VISIT(value);
  VISIT_REP(usage_timestamp);
  VISIT(profile);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillWalletCredentialSpecifics& proto) {
  VISIT(instrument_id);
  VISIT_SECRET(cvc);
  VISIT(last_updated_time_unix_epoch_millis);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillWalletUsageSpecifics& proto) {
  VISIT(guid);
  VISIT(virtual_card_usage_data);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AutofillWalletUsageSpecifics::VirtualCardUsageData& proto) {
  VISIT(instrument_id);
  VISIT(virtual_card_last_four);
  VISIT(merchant_url);
  VISIT(merchant_app_package);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillWalletSpecifics& proto) {
  VISIT_ENUM(type);
  VISIT(masked_card);
  VISIT(address);
  VISIT(customer_data);
  VISIT(cloud_token_data);
  VISIT(payment_instrument);
}

VISIT_PROTO_FIELDS(const sync_pb::BookmarkSpecifics& proto) {
  VISIT(url);
  VISIT_BYTES(favicon);
  VISIT(guid);
  VISIT(legacy_canonicalized_title);
  VISIT(creation_time_us);
  VISIT(icon_url);
  VISIT_REP(meta_info);
  VISIT(full_title);
  VISIT(parent_guid);
  VISIT_ENUM(type);
  VISIT(unique_position);
  VISIT(last_used_time_us);
}

VISIT_PROTO_FIELDS(const sync_pb::ChromiumExtensionsActivity& proto) {
  VISIT(extension_id);
  VISIT(bookmark_writes_since_last_commit);
}

VISIT_PROTO_FIELDS(const sync_pb::CollaborationGroupSpecifics& proto) {
  VISIT(collaboration_id);
  VISIT(changed_at_timestamp_millis_since_unix_epoch);
  VISIT(consistency_token);
}

VISIT_PROTO_FIELDS(const sync_pb::ComparisonData& proto) {
  VISIT(url);
}

VISIT_PROTO_FIELDS(const sync_pb::ProductComparison& proto) {
  VISIT(name);
}

VISIT_PROTO_FIELDS(const sync_pb::ProductComparisonItem& proto) {
  VISIT(product_comparison_uuid);
  VISIT(url);
  VISIT(unique_position);
  VISIT(title);
}

VISIT_PROTO_FIELDS(const sync_pb::ProductComparisonSpecifics& proto) {
  VISIT(uuid);
  VISIT(creation_time_unix_epoch_millis);
  VISIT(update_time_unix_epoch_millis);
  VISIT(name);
  VISIT_REP(data);
  VISIT(product_comparison);
  VISIT(product_comparison_item);
}

VISIT_PROTO_FIELDS(const sync_pb::ContactInfoSpecifics& proto) {
  VISIT(guid);
  VISIT_ENUM(address_type);
  VISIT(use_count);
  VISIT(use_date_unix_epoch_seconds);
  VISIT(use_date2_unix_epoch_seconds);
  VISIT(use_date3_unix_epoch_seconds);
  VISIT(date_modified_unix_epoch_seconds);
  VISIT(language_code);
  VISIT(profile_label);
  VISIT(initial_creator_id);
  VISIT(last_modifier_id);
  VISIT(name_first);
  VISIT(name_middle);
  VISIT(name_last);
  VISIT(name_last_first);
  VISIT(name_last_conjunction);
  VISIT(name_last_second);
  VISIT(name_full);
  VISIT(email_address);
  VISIT(company_name);
  VISIT(address_city);
  VISIT(address_state);
  VISIT(address_zip);
  VISIT(address_country);
  VISIT(address_street_address);
  VISIT(address_sorting_code);
  VISIT(address_dependent_locality);
  VISIT(address_thoroughfare_name);
  VISIT(address_thoroughfare_number);
  VISIT(address_subpremise_name);
  VISIT(address_apt);
  VISIT(address_apt_num);
  VISIT(address_apt_type);
  VISIT(address_floor);
  VISIT(address_landmark);
  VISIT(address_between_streets);
  VISIT(address_admin_level_2);
  VISIT(phone_home_whole_number);
  VISIT(address_street_location);
  VISIT(address_overflow);
  VISIT(address_between_streets_1);
  VISIT(address_between_streets_2);
  VISIT(address_between_streets_or_landmark);
  VISIT(address_overflow_and_landmark);
  VISIT(address_street_location_and_locality);
  VISIT(address_thoroughfare_number_and_apt);
}

VISIT_PROTO_FIELDS(const sync_pb::ContactInfoSpecifics::Observation& proto) {
  VISIT(type);
  VISIT(form_hash);
}

VISIT_PROTO_FIELDS(const sync_pb::ContactInfoSpecifics::TokenMetadata& proto) {
  VISIT_ENUM(status);
  VISIT_REP(observations);
  VISIT(value_hash);
}

VISIT_PROTO_FIELDS(const sync_pb::ContactInfoSpecifics::StringToken& proto) {
  VISIT(value);
  VISIT(metadata);
}

VISIT_PROTO_FIELDS(const sync_pb::CookieSpecifics& proto) {
  VISIT(unique_key);
  VISIT(name);
  VISIT(value);
  VISIT(domain);
  VISIT(path);
  VISIT(creation_time_windows_epoch_micros);
  VISIT(expiry_time_windows_epoch_micros);
  VISIT(last_access_time_windows_epoch_micros);
  VISIT(last_update_time_windows_epoch_micros);
  VISIT(secure);
  VISIT(httponly);
  VISIT_ENUM(site_restrictions);
  VISIT_ENUM(priority);
  VISIT_ENUM(source_scheme);
  VISIT(partition_key);
  VISIT(source_port);
  VISIT_ENUM(source_type);
}

VISIT_PROTO_FIELDS(
    const sync_pb::CookieSpecifics::SerializedCookiePartitionKey& proto) {
  VISIT(top_level_site);
  VISIT(has_cross_site_ancestor);
}

VISIT_PROTO_FIELDS(const sync_pb::CustomNudgeDelay& proto) {
  VISIT(datatype_id);
  VISIT(delay_ms);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientCommand& proto) {
  VISIT(set_sync_poll_interval);
  VISIT(max_commit_batch_size);
  VISIT(throttle_delay_seconds);
  VISIT(client_invalidation_hint_buffer_size);
  VISIT(gu_retry_delay_seconds);
  VISIT_REP(custom_nudge_delays);
  VISIT(extension_types_max_tokens);
  VISIT(extension_types_refill_interval_seconds);
  VISIT(extension_types_depleted_quota_nudge_delay_seconds);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientConfigParams& proto) {
  VISIT_REP(enabled_type_ids);
  VISIT(tabs_datatype_enabled);
  VISIT(cookie_jar_mismatch);
  VISIT(single_client);
  VISIT_REP(devices_fcm_registration_tokens);
  VISIT(single_client_with_standalone_invalidations);
  VISIT_REP(fcm_registration_tokens_for_interested_clients);
  VISIT(single_client_with_old_invalidations);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientStatus& proto) {
  VISIT(hierarchy_conflict_detected);
  VISIT(is_sync_feature_enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientToServerMessage& proto) {
  VISIT(share);
  VISIT(protocol_version);
  VISIT(commit);
  VISIT(get_updates);
  VISIT(store_birthday);
  VISIT(debug_info);
  VISIT(client_status);
  VISIT(invalidator_client_id);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientToServerResponse& proto) {
  VISIT(commit);
  VISIT(get_updates);
  VISIT(error);
  VISIT_ENUM(error_code);
  VISIT(error_message);
  VISIT(store_birthday);
  VISIT(client_command);
  VISIT_REP(migrated_data_type_id);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientToServerResponse::Error& proto) {
  VISIT_ENUM(error_type);
  VISIT(error_description);
  VISIT_ENUM(action);
  VISIT_REP(error_data_type_ids);
}

VISIT_PROTO_FIELDS(const sync_pb::CommitMessage& proto) {
  VISIT_REP(entries);
  VISIT(cache_guid);
  VISIT_REP(extensions_activity);
  VISIT(config_params);
  VISIT_REP(client_contexts);
}

VISIT_PROTO_FIELDS(const sync_pb::CommitResponse& proto) {
  VISIT_REP(entryresponse);
}

VISIT_PROTO_FIELDS(const sync_pb::CommitResponse::EntryResponse& proto) {
  VISIT_ENUM(response_type);
  VISIT(id_string);
  VISIT(version);
  VISIT(error_message);
  VISIT(mtime);
}

VISIT_PROTO_FIELDS(const sync_pb::DataTypeContext& proto) {
  VISIT(data_type_id);
  VISIT(context);
  VISIT(version);
}

VISIT_PROTO_FIELDS(const sync_pb::DataTypeProgressMarker& proto) {
  VISIT(data_type_id);
  VISIT_BYTES(token);
  VISIT(get_update_triggers);
  VISIT(gc_directive);
}

VISIT_PROTO_FIELDS(const sync_pb::GarbageCollectionDirective& proto) {
  VISIT(version_watermark);
  VISIT(collaboration_gc);
}

VISIT_PROTO_FIELDS(
    const sync_pb::GarbageCollectionDirective::CollaborationGarbageCollection&
        proto) {
  VISIT_REP(active_collaboration_ids);
}

VISIT_PROTO_FIELDS(const sync_pb::DebugEventInfo& proto) {
  VISIT_ENUM(singleton_event);
  VISIT(sync_cycle_completed_event_info);
  VISIT(nudging_datatype);
  VISIT_REP(datatypes_notified_from_server);
}

VISIT_PROTO_FIELDS(const sync_pb::DebugInfo& proto) {
  VISIT_REP(events);
  VISIT(cryptographer_ready);
  VISIT(cryptographer_has_pending_keys);
  VISIT(events_dropped);
}

VISIT_PROTO_FIELDS(const sync_pb::DeletionOrigin& proto) {
  VISIT(chromium_version);
  VISIT(google_play_services_apk_version_name);
  VISIT(file_name_hash);
  VISIT(file_line_number);
  VISIT(file_name_possibly_truncated);
  VISIT(unique_source_tag_no_pii_possibly_truncated);
}

VISIT_PROTO_FIELDS(const sync_pb::DeviceInfoSpecifics& proto) {
  VISIT(cache_guid);
  VISIT(client_name);
  VISIT_ENUM(device_type);
  VISIT(sync_user_agent);
  VISIT(chrome_version);
  VISIT(signin_scoped_device_id);
  VISIT(model);
  VISIT(manufacturer);
  VISIT(last_updated_timestamp);
  VISIT(feature_fields);
  VISIT(sharing_fields);
  VISIT(invalidation_fields);
  VISIT(paask_fields);
  VISIT(full_hardware_class);
  VISIT(chrome_version_info);
  VISIT(google_play_services_version_info);
  VISIT_ENUM(os_type);
  VISIT_ENUM(device_form_factor);
}

VISIT_PROTO_FIELDS(const sync_pb::FeatureSpecificFields& proto) {
  VISIT(send_tab_to_self_receiving_enabled);
  VISIT_ENUM(send_tab_to_self_receiving_type);
  VISIT(floating_workspace_last_signin_time_windows_epoch_micros);
}

VISIT_PROTO_FIELDS(const sync_pb::SharingSpecificFields& proto) {
  VISIT(vapid_fcm_token);
  VISIT_BYTES(vapid_p256dh);
  VISIT_BYTES(vapid_auth_secret);
  VISIT_REP(enabled_features);
  VISIT(sender_id_fcm_token_v2);
  VISIT_BYTES(sender_id_p256dh_v2);
  VISIT_BYTES(sender_id_auth_secret_v2);
  VISIT(chime_representative_target_id);
}

VISIT_PROTO_FIELDS(const sync_pb::PhoneAsASecurityKeySpecificFields& proto) {
  VISIT(tunnel_server_domain);
  VISIT_BYTES(contact_id);
  VISIT(id);
  VISIT_BYTES(peer_public_key_x962);
  VISIT_BYTES(google_credential_id);
  // |secret| is deliberately omitted to avoid including sensitive information
  // in debugging output, which might be included in bug reports etc.
}

VISIT_PROTO_FIELDS(const sync_pb::ChromeVersionInfo& proto) {
  VISIT(version_number);
}

VISIT_PROTO_FIELDS(const sync_pb::GooglePlayServicesVersionInfo& proto) {
  VISIT(apk_version_name);
}

VISIT_PROTO_FIELDS(const sync_pb::DictionarySpecifics& proto) {
  VISIT(word);
}

VISIT_PROTO_FIELDS(const sync_pb::EncryptedData& proto) {
  VISIT(key_name);
  VISIT_BYTES(blob);
}

VISIT_PROTO_FIELDS(const sync_pb::EntityMetadata& proto) {
  VISIT(client_tag_hash);
  VISIT(server_id);
  VISIT(is_deleted);
  VISIT(sequence_number);
  VISIT(acked_sequence_number);
  VISIT(server_version);
  VISIT(creation_time);
  VISIT(modification_time);
  VISIT(specifics_hash);
  VISIT(base_specifics_hash);
  VISIT(possibly_trimmed_base_specifics);
  VISIT(deleted_by_version);
  VISIT(collaboration);
  VISIT(deletion_origin);
}

VISIT_PROTO_FIELDS(
    const sync_pb::EntityMetadata::CollaborationMetadata& proto) {
  VISIT(collaboration_id);
}

VISIT_PROTO_FIELDS(const sync_pb::EntitySpecifics& proto) {
  static_assert(53 == GetNumDataTypes(),
                "When adding a new protocol type, you will likely need to add "
                "it here as well.");
  VISIT(encrypted);
  VISIT(app);
  VISIT(app_list);
  VISIT(app_setting);
  VISIT(arc_package);
  VISIT(autofill);
  VISIT(autofill_offer);
  VISIT(autofill_profile);
  VISIT(autofill_wallet);
  VISIT(autofill_wallet_credential);
  VISIT(autofill_wallet_usage);
  VISIT(bookmark);
  VISIT(collaboration_group);
  VISIT(contact_info);
  VISIT(cookie);
  VISIT(device_info);
  VISIT(dictionary);
  VISIT(extension);
  VISIT(extension_setting);
  VISIT(history);
  VISIT(history_delete_directive);
  VISIT(incoming_password_sharing_invitation);
  VISIT(managed_user_setting);
  VISIT(nigori);
  VISIT(os_preference);
  VISIT(os_priority_preference);
  VISIT(outgoing_password_sharing_invitation);
  VISIT(password);
  VISIT(plus_address);
  VISIT(plus_address_setting);
  VISIT(power_bookmark);
  VISIT(preference);
  VISIT(printer);
  VISIT(printers_authorization_server);
  VISIT(priority_preference);
  VISIT(product_comparison);
  VISIT(reading_list);
  VISIT(saved_tab_group);
  VISIT(search_engine);
  VISIT(security_event);
  VISIT(send_tab_to_self);
  VISIT(session);
  VISIT(shared_tab_group_data);
  VISIT(sharing_message);
  VISIT(theme);
  VISIT(typed_url);
  VISIT(user_consent);
  VISIT(user_event);
  VISIT(wallet_metadata);
  VISIT(web_apk);
  VISIT(web_app);
  VISIT(webauthn_credential);
  VISIT(wifi_configuration);
  VISIT(workspace_desk);
  VISIT(webauthn_credential);
}

VISIT_PROTO_FIELDS(const sync_pb::ExtensionSettingSpecifics& proto) {
  VISIT(extension_id);
  VISIT(key);
  VISIT(value);
}

VISIT_PROTO_FIELDS(const sync_pb::ExtensionSpecifics& proto) {
  VISIT(id);
  VISIT(version);
  VISIT(update_url);
  VISIT(enabled);
  VISIT(incognito_enabled);
  VISIT(remote_install);
  VISIT(all_urls_enabled);
  VISIT(disable_reasons);
}

VISIT_PROTO_FIELDS(const sync_pb::GetUpdateTriggers& proto) {
  VISIT_REP(notification_hint);
  VISIT(client_dropped_hints);
  VISIT(invalidations_out_of_sync);
  VISIT(local_modification_nudges);
  VISIT(datatype_refresh_nudges);
  VISIT(server_dropped_hints);
  VISIT(initial_sync_in_progress);
  VISIT(sync_for_resolve_conflict_in_progress);
}

VISIT_PROTO_FIELDS(const sync_pb::GetUpdatesCallerInfo& proto) {
  VISIT(notifications_enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::GetUpdatesMessage& proto) {
  VISIT(caller_info);
  VISIT(fetch_folders);
  VISIT_REP(from_progress_marker);
  VISIT(streaming);
  VISIT(need_encryption_key);
  VISIT_ENUM(get_updates_origin);
  VISIT(is_retry);
  VISIT_REP(client_contexts);
}

VISIT_PROTO_FIELDS(const sync_pb::GetUpdatesResponse& proto) {
  VISIT_REP(entries)
  VISIT(changes_remaining);
  VISIT_REP(new_progress_marker);
  VISIT_REP(context_mutations);
}

VISIT_PROTO_FIELDS(const sync_pb::GlobalIdDirective& proto) {
  VISIT_REP(global_id);
  VISIT(start_time_usec);
  VISIT(end_time_usec);
}

VISIT_PROTO_FIELDS(const sync_pb::HistoryDeleteDirectiveSpecifics& proto) {
  VISIT(global_id_directive);
  VISIT(time_range_directive);
  VISIT(url_directive);
}

VISIT_PROTO_FIELDS(
    const sync_pb::IncomingPasswordSharingInvitationSpecifics& proto) {
  VISIT(guid);
  VISIT(sender_info);
  VISIT_BYTES(encrypted_password_sharing_invitation_data);
  VISIT(client_only_unencrypted_data);
  VISIT(recipient_key_version);
}

VISIT_PROTO_FIELDS(const sync_pb::InvalidationSpecificFields& proto) {
  VISIT(instance_id_token);
  VISIT_REP(interested_data_type_ids);
}

VISIT_PROTO_FIELDS(const sync_pb::LinkedAppIconInfo& proto) {
  VISIT(url);
  VISIT(size);
}

VISIT_PROTO_FIELDS(const sync_pb::ManagedUserSettingSpecifics& proto) {
  VISIT(name);
  VISIT(value);
}

VISIT_PROTO_FIELDS(const sync_pb::MetaInfo& proto) {
  VISIT(key);
  VISIT(value);
}

VISIT_PROTO_FIELDS(const sync_pb::DataTypeState& proto) {
  VISIT(progress_marker);
  VISIT(type_context);
  VISIT(encryption_key_name);
  VISIT_ENUM(initial_sync_state);
  VISIT(cache_guid);
  VISIT(authenticated_account_id);
  VISIT_REP(invalidations);
  VISIT(notes_enabled_before_initial_sync_for_passwords);
}

VISIT_PROTO_FIELDS(const sync_pb::DataTypeState::Invalidation& proto) {
  VISIT(hint);
  VISIT(version);
}

VISIT_PROTO_FIELDS(const sync_pb::NavigationRedirect& proto) {
  VISIT(url);
}

VISIT_PROTO_FIELDS(const sync_pb::ReplacedNavigation& proto) {
  VISIT(first_committed_url);
  VISIT(first_timestamp_msec);
  VISIT_ENUM(first_page_transition);
}

VISIT_PROTO_FIELDS(const sync_pb::CryptographerData& proto) {
  VISIT(key_bag);
  VISIT(default_key_name);
  VISIT(cross_user_sharing_keys);
}

VISIT_PROTO_FIELDS(const sync_pb::CrossUserSharingKeys& proto) {
  VISIT_REP(private_key);
}

VISIT_PROTO_FIELDS(const sync_pb::CustomPassphraseKeyDerivationParams& proto) {
  VISIT(custom_passphrase_key_derivation_method);
  VISIT(custom_passphrase_key_derivation_salt);
}

VISIT_PROTO_FIELDS(const sync_pb::NigoriModel& proto) {
  VISIT(cryptographer_data);
  VISIT(current_keystore_key_name);
  VISIT(pending_keys);
  VISIT(passphrase_type);
  VISIT(keystore_migration_time);
  VISIT(custom_passphrase_time);
  VISIT(custom_passphrase_key_derivation_params);
  VISIT(encrypt_everything);
  VISIT_REP(encrypted_types_specifics_field_number);
  VISIT_REP(keystore_key);
  VISIT(pending_keystore_decryptor_token);
  VISIT(last_default_trusted_vault_key_name);
  VISIT(trusted_vault_debug_info);
  VISIT(cross_user_sharing_public_key);
}

VISIT_PROTO_FIELDS(const sync_pb::NigoriLocalData& proto) {
  VISIT(data_type_state);
  VISIT(entity_metadata);
  VISIT(nigori_model);
}

VISIT_PROTO_FIELDS(const sync_pb::CrossUserSharingPublicKey& proto) {
  VISIT(version);
  VISIT_BYTES(x25519_public_key);
}

VISIT_PROTO_FIELDS(const sync_pb::CrossUserSharingPrivateKey& proto) {
  VISIT(version);
  VISIT(x25519_private_key);
}

VISIT_PROTO_FIELDS(const sync_pb::NigoriSpecifics& proto) {
  VISIT(encryption_keybag);
  VISIT(keybag_is_frozen);
  VISIT(encrypt_everything);
  VISIT(server_only_was_missing_keystore_migration_time);
  VISIT(sync_tab_favicons);
  VISIT(passphrase_type);
  VISIT(keystore_decryptor_token);
  VISIT(keystore_migration_time);
  VISIT(custom_passphrase_time);
  VISIT(custom_passphrase_key_derivation_method);
  VISIT(custom_passphrase_key_derivation_salt);
  VISIT(trusted_vault_debug_info);
  VISIT(cross_user_sharing_public_key);
}

VISIT_PROTO_FIELDS(const sync_pb::WebauthnCredentialSpecifics& proto) {
  VISIT_BYTES(sync_id);
  VISIT_BYTES(credential_id);
  VISIT(rp_id);
  VISIT_BYTES(user_id);
  VISIT_REP_BYTES(newly_shadowed_credential_ids);
  VISIT(creation_time);
  VISIT(user_name);
  VISIT(user_display_name);
  VISIT(third_party_payments_support);
  VISIT(last_used_time_windows_epoch_micros);
  VISIT(key_version);
  VISIT_SECRET(private_key);
  VISIT_SECRET(encrypted);
  VISIT(edited_by_user);
  VISIT(hidden);
}

VISIT_PROTO_FIELDS(const sync_pb::HistorySpecifics::PageTransition& proto) {
  VISIT_ENUM(core_transition);
  VISIT(blocked);
  VISIT(forward_back);
  VISIT(from_address_bar);
  VISIT(home_page);
}

VISIT_PROTO_FIELDS(const sync_pb::HistorySpecifics::RedirectEntry& proto) {
  VISIT(originator_visit_id);
  VISIT(url);
  VISIT(title);
  VISIT(hidden);
  VISIT_ENUM(redirect_type);
}

VISIT_PROTO_FIELDS(const sync_pb::HistorySpecifics::Category& proto) {
  VISIT(id);
  VISIT(weight);
}

VISIT_PROTO_FIELDS(const sync_pb::HistorySpecifics& proto) {
  VISIT(visit_time_windows_epoch_micros);
  VISIT(originator_cache_guid);
  VISIT_REP(redirect_entries);
  VISIT(redirect_chain_start_incomplete);
  VISIT(redirect_chain_end_incomplete);
  VISIT(redirect_chain_middle_trimmed);
  VISIT(page_transition);
  VISIT(originator_referring_visit_id);
  VISIT(originator_opener_visit_id);
  VISIT(originator_cluster_id);
  VISIT(visit_duration_micros);
  VISIT_ENUM(browser_type);
  VISIT(window_id);
  VISIT(tab_id);
  VISIT(task_id);
  VISIT(root_task_id);
  VISIT(parent_task_id);
  VISIT(http_response_code);
  VISIT(page_language);
  VISIT_ENUM(password_state);
  VISIT(favicon_url);
  VISIT(referrer_url);
  VISIT(has_url_keyed_image);
  VISIT_REP(categories);
  VISIT_REP(related_searches);
  VISIT(app_id);
}

VISIT_PROTO_FIELDS(
    const sync_pb::TrustedVaultAutoUpgradeExperimentGroup& proto) {
  VISIT(cohort);
  VISIT_ENUM(type);
  VISIT(type_index);
}

VISIT_PROTO_FIELDS(
    const sync_pb::NigoriSpecifics::TrustedVaultDebugInfo& proto) {
  VISIT(migration_time);
  VISIT(key_version);
  VISIT(auto_upgrade_experiment_group);
  VISIT(auto_upgrade_experiment_group_modification_time_unix_epoch_millis);
}

VISIT_PROTO_FIELDS(const sync_pb::OsPreferenceSpecifics& proto) {
  VISIT(preference);
}

VISIT_PROTO_FIELDS(const sync_pb::OsPriorityPreferenceSpecifics& proto) {
  VISIT(preference);
}

VISIT_PROTO_FIELDS(
    const sync_pb::OutgoingPasswordSharingInvitationSpecifics& proto) {
  VISIT(guid);
  VISIT(recipient_user_id);
  VISIT_BYTES(encrypted_password_sharing_invitation_data);
  VISIT(client_only_unencrypted_data);
  VISIT(recipient_key_version);
  VISIT(sender_key_version);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSharingInvitationData& proto) {
  VISIT(password_group_data);
}

VISIT_PROTO_FIELDS(
    const sync_pb::PasswordSharingInvitationData::PasswordGroupData& proto) {
  VISIT(username_value);
  VISIT(password_value);
  VISIT_REP(element_data);
}

VISIT_PROTO_FIELDS(
    const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
        proto) {
  VISIT(scheme);
  VISIT(signon_realm);
  VISIT(origin);
  VISIT(username_element);
  VISIT(password_element);
  VISIT(display_name);
  VISIT(avatar_url);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecifics& proto) {
  VISIT(encrypted);
  VISIT(unencrypted_metadata);
  VISIT(client_only_encrypted_data);
  VISIT(encrypted_notes_backup);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecificsData& proto) {
  VISIT(scheme);
  VISIT(signon_realm);
  VISIT(origin);
  VISIT(action);
  VISIT(username_element);
  VISIT(username_value);
  VISIT(password_element);
  VISIT(password_value);
  VISIT(preferred);
  VISIT(date_created);
  VISIT(blacklisted);
  VISIT(type);
  VISIT(times_used);
  VISIT(display_name);
  VISIT(avatar_url);
  VISIT(federation_url);
  VISIT(date_last_used);
  VISIT(password_issues);
  VISIT(date_password_modified_windows_epoch_micros);
  VISIT(notes);
  VISIT(sender_email);
  VISIT(sender_name);
  VISIT(date_received_windows_epoch_micros);
  VISIT(sharing_notification_displayed);
  VISIT(sender_profile_image_url);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordIssues& proto) {
  VISIT(leaked_password_issue);
  VISIT(reused_password_issue);
  VISIT(weak_password_issue);
  VISIT(phished_password_issue);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordIssues_PasswordIssue& proto) {
  VISIT(date_first_detection_windows_epoch_micros);
  VISIT(is_muted);
  VISIT(trigger_notification_from_backend_on_detection);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecificsData_Notes& proto) {
  VISIT_REP(note);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecificsData_Notes_Note& proto) {
  VISIT(unique_display_name);
  VISIT(value);
  VISIT(date_created_windows_epoch_micros);
  VISIT(hide_by_default);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecificsMetadata& proto) {
  VISIT(url);
  VISIT(blacklisted);
  VISIT(date_last_used_windows_epoch_micros);
  VISIT(password_issues);
  VISIT(type);
}

VISIT_PROTO_FIELDS(const sync_pb::PowerBookmarkSpecifics& proto) {
  VISIT(guid);
  VISIT(url);
  VISIT_ENUM(power_type);
  VISIT(creation_time_usec);
  VISIT(update_time_usec);
  VISIT(power_entity);
}

VISIT_PROTO_FIELDS(const sync_pb::PowerEntity& proto) {
  VISIT(note_entity);
}

VISIT_PROTO_FIELDS(const sync_pb::NoteEntity& proto) {
  VISIT(plain_text);
  VISIT(rich_text);
  VISIT_ENUM(target_type);
  VISIT(current_note_version);
}

VISIT_PROTO_FIELDS(const sync_pb::PersistedEntityData& proto) {
  VISIT(name);
  VISIT(specifics);
}

VISIT_PROTO_FIELDS(const sync_pb::PlusAddressSettingSpecifics& proto) {
  VISIT(name);
  VISIT(bool_value);
  VISIT(string_value);
  VISIT(int_value);
}

VISIT_PROTO_FIELDS(const sync_pb::PlusAddressSpecifics& proto) {
  VISIT(profile_id);
  VISIT(facet);
  VISIT(plus_email);
}

VISIT_PROTO_FIELDS(const sync_pb::PlusEmail& proto) {
  VISIT(email_address);
}

VISIT_PROTO_FIELDS(const sync_pb::PreferenceSpecifics& proto) {
  VISIT(name);
  VISIT(value);
}

VISIT_PROTO_FIELDS(const sync_pb::PrinterPPDReference& proto) {
  VISIT(user_supplied_ppd_url);
  VISIT(effective_make_and_model);
  VISIT(autoconf);
}

VISIT_PROTO_FIELDS(const sync_pb::PrinterSpecifics& proto) {
  VISIT(id);
  VISIT(display_name);
  VISIT(description);
  VISIT(manufacturer);
  VISIT(model);
  VISIT(uri);
  VISIT(uuid);
  VISIT(ppd_reference);
  VISIT(make_and_model);
}

VISIT_PROTO_FIELDS(const sync_pb::PrintersAuthorizationServerSpecifics& proto) {
  VISIT(uri);
}

VISIT_PROTO_FIELDS(const sync_pb::PriorityPreferenceSpecifics& proto) {
  VISIT(preference);
}

VISIT_PROTO_FIELDS(const sync_pb::ReadingListSpecifics& proto) {
  VISIT(entry_id);
  VISIT(title);
  VISIT(url);
  VISIT(creation_time_us);
  VISIT(update_time_us);
  VISIT_ENUM(status);
  VISIT(first_read_time_us);
  VISIT(update_title_time_us);
  VISIT(estimated_read_time_seconds);
}

VISIT_PROTO_FIELDS(const sync_pb::SavedTabGroupSpecifics& proto) {
  VISIT(guid);
  VISIT(creation_time_windows_epoch_micros);
  VISIT(update_time_windows_epoch_micros);
  VISIT(group);
  VISIT(tab);
  VISIT(attribution_metadata);
}

VISIT_PROTO_FIELDS(const sync_pb::SavedTabGroup& proto) {
  VISIT(position);
  VISIT(title);
  VISIT_ENUM(color);
  VISIT(pinned_position);
}

VISIT_PROTO_FIELDS(const sync_pb::SavedTabGroupTab& proto) {
  VISIT(position);
  VISIT(group_guid);
  VISIT(url);
  VISIT(title);
}

VISIT_PROTO_FIELDS(const sync_pb::AttributionMetadata& proto) {
  VISIT(created);
  VISIT(updated);
}

VISIT_PROTO_FIELDS(const sync_pb::AttributionMetadata::Attribution& proto) {
  VISIT(device_info);
}

VISIT_PROTO_FIELDS(
    const sync_pb::AttributionMetadata::Attribution::AttributionDeviceInfo&
        proto) {
  VISIT(cache_guid);
}

VISIT_PROTO_FIELDS(const sync_pb::SearchEngineSpecifics& proto) {
  VISIT(short_name);
  VISIT(keyword);
  VISIT(favicon_url);
  VISIT(url);
  VISIT(safe_for_autoreplace);
  VISIT(originating_url);
  VISIT(date_created);
  VISIT(input_encodings);
  VISIT(suggestions_url);
  VISIT(prepopulate_id);
  VISIT(autogenerate_keyword);
  VISIT(last_modified);
  VISIT(sync_guid);
  VISIT_REP(alternate_urls);
  VISIT(image_url);
  VISIT(search_url_post_params);
  VISIT(suggestions_url_post_params);
  VISIT(image_url_post_params);
  VISIT(new_tab_url);
  VISIT_ENUM(is_active);
  VISIT(starter_pack_id);
}

VISIT_PROTO_FIELDS(const sync_pb::SendTabToSelfPush& proto) {
  VISIT(title);
  VISIT(text);
  VISIT_REP(icon);
  VISIT(favicon);
  VISIT(destination_url);
  VISIT(placeholder_title);
  VISIT(placeholder_body);
  VISIT(entry_unique_guid);
}

VISIT_PROTO_FIELDS(const sync_pb::SendTabToSelfPush::Image& proto) {
  VISIT(url);
  VISIT(alt_text);
}

VISIT_PROTO_FIELDS(const sync_pb::SendTabToSelfSpecifics& proto) {
  VISIT(guid);
  VISIT(title);
  VISIT(url);
  VISIT(shared_time_usec);
  VISIT(device_name);
  VISIT(target_device_sync_cache_guid);
  VISIT(opened);
  VISIT(notification_dismissed);
}

VISIT_PROTO_FIELDS(const sync_pb::SessionHeader& proto) {
  VISIT(session_start_time_unix_epoch_millis);
  VISIT_REP(window);
  VISIT(client_name);
  VISIT_ENUM(device_type);
  VISIT_ENUM(device_form_factor);
}

VISIT_PROTO_FIELDS(const sync_pb::SessionSpecifics& proto) {
  VISIT(session_tag);
  VISIT(header);
  VISIT(tab);
  VISIT(tab_node_id);
}

VISIT_PROTO_FIELDS(const sync_pb::SessionTab& proto) {
  VISIT(tab_id);
  VISIT(window_id);
  VISIT(tab_visual_index);
  VISIT(current_navigation_index);
  VISIT(pinned);
  VISIT(extension_app_id);
  VISIT_REP(navigation);
  VISIT_BYTES(favicon);
  VISIT_ENUM(favicon_type);
  VISIT(favicon_source);
  VISIT_REP(variation_id);
  VISIT_ENUM(browser_type);
  VISIT(last_active_time_unix_epoch_millis);
}

VISIT_PROTO_FIELDS(const sync_pb::SessionWindow& proto) {
  VISIT(window_id);
  VISIT(selected_tab_index);
  VISIT_REP(tab);
  VISIT_ENUM(browser_type);
}

VISIT_PROTO_FIELDS(const sync_pb::SharedTabGroup& proto) {
  VISIT(title);
  VISIT_ENUM(color);
  VISIT(originating_tab_group_guid);
}

VISIT_PROTO_FIELDS(const sync_pb::SharedTab& proto) {
  VISIT(url);
  VISIT(title);
  VISIT(favicon_url);
  VISIT(shared_tab_group_guid);
  VISIT(unique_position);
}

VISIT_PROTO_FIELDS(const sync_pb::SharedTabGroupDataSpecifics& proto) {
  VISIT(guid);
  VISIT(tab_group);
  VISIT(tab);
  VISIT(update_time_windows_epoch_micros);
}

VISIT_PROTO_FIELDS(const sync_pb::SharingMessageSpecifics& proto) {
  VISIT(message_id);
  VISIT(channel_configuration);
  VISIT_BYTES(payload);
  VISIT(unencrypted_payload);
}

VISIT_PROTO_FIELDS(const sync_pb::SharingMessageSpecifics::
                       ChannelConfiguration::FCMChannelConfiguration& proto) {
  VISIT(token);
  VISIT(ttl);
  VISIT(priority);
}

VISIT_PROTO_FIELDS(const sync_pb::SharingMessageSpecifics::
                       ChannelConfiguration::ChimeChannelConfiguration& proto) {
  VISIT_BYTES(device_token);
  VISIT_ENUM(channel_type);
  VISIT(type_id);
  VISIT(representative_target_id);
}

VISIT_PROTO_FIELDS(
    const sync_pb::SharingMessageSpecifics::ChannelConfiguration& proto) {
  VISIT(fcm);
  VISIT_BYTES(server);
  VISIT(chime);
}

VISIT_PROTO_FIELDS(const sync_pb::SyncCycleCompletedEventInfo& proto) {
  VISIT(num_encryption_conflicts);
  VISIT(num_hierarchy_conflicts);
  VISIT(num_server_conflicts);
  VISIT(num_updates_downloaded);
  VISIT(num_reflected_updates_downloaded);
  VISIT(caller_info);
  VISIT_ENUM(get_updates_origin);
}

VISIT_PROTO_FIELDS(const sync_pb::SyncEntity& proto) {
  VISIT(id_string);
  VISIT(parent_id_string);
  VISIT(version);
  VISIT(mtime);
  VISIT(ctime);
  VISIT(name);
  VISIT(non_unique_name);
  VISIT(server_defined_unique_tag);
  VISIT(unique_position);
  VISIT(deleted);
  VISIT(originator_cache_guid);
  VISIT(originator_client_item_id);
  VISIT(specifics);
  VISIT(folder);
  VISIT(client_tag_hash);
  VISIT(collaboration);
  VISIT(deletion_origin);
}

VISIT_PROTO_FIELDS(const sync_pb::SyncEntity::CollaborationMetadata& proto) {
  VISIT(collaboration_id);
  VISIT(attribution_metadata);
}

VISIT_PROTO_FIELDS(
    const sync_pb::SyncEntity::CollaborationMetadata::AttributionMetadata&
        proto) {
  VISIT(created);
  VISIT(updated);
}

VISIT_PROTO_FIELDS(const sync_pb::SyncEntity::CollaborationMetadata::
                       AttributionMetadata::Attribution& proto) {
  VISIT(user_info);
}

VISIT_PROTO_FIELDS(
    const sync_pb::SyncEntity::CollaborationMetadata::AttributionMetadata::
        Attribution::AttributionUserInfo& proto) {
  VISIT(gaia_id);
}

VISIT_PROTO_FIELDS(const sync_pb::SyncInvalidationsPayload& proto) {
  VISIT_REP(data_type_invalidations);
  VISIT_BYTES(hint);
  VISIT(version);
}

VISIT_PROTO_FIELDS(
    const sync_pb::SyncInvalidationsPayload::DataTypeInvalidation& proto) {
  VISIT(data_type_id);
}

VISIT_PROTO_FIELDS(const sync_pb::SecurityEventSpecifics& proto) {
  VISIT(gaia_password_reuse_event);
  VISIT(event_time_usec);
}

VISIT_PROTO_FIELDS(const sync_pb::GaiaPasswordReuse& proto) {
  VISIT(reuse_detected);
  VISIT(reuse_lookup);
  VISIT(dialog_interaction);
}

VISIT_PROTO_FIELDS(
    const sync_pb::GaiaPasswordReuse::PasswordReuseDetected& proto) {
  VISIT(status);
}

VISIT_PROTO_FIELDS(
    const sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus&
        proto) {
  VISIT(enabled);
  VISIT_ENUM(safe_browsing_reporting_population);
}

VISIT_PROTO_FIELDS(
    const sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction& proto) {
  VISIT_ENUM(interaction_result);
}

VISIT_PROTO_FIELDS(
    const sync_pb::GaiaPasswordReuse::PasswordReuseLookup& proto) {
  VISIT_ENUM(lookup_result);
  VISIT_ENUM(verdict);
  VISIT(verdict_token);
}

VISIT_PROTO_FIELDS(const sync_pb::UserDisplayInfo& proto) {
  VISIT(email);
  VISIT(display_name);
  VISIT(profile_image_url);
}

VISIT_PROTO_FIELDS(const sync_pb::UserInfo& proto) {
  VISIT(user_id);
  VISIT(user_display_info);
  VISIT(cross_user_sharing_public_key);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserEventSpecifics::GaiaPasswordCaptured& proto) {
  VISIT_ENUM(event_trigger);
}

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics::FlocIdComputed& proto) {
  VISIT(floc_id);
}

VISIT_PROTO_FIELDS(const sync_pb::TabNavigation& proto) {
  VISIT(virtual_url);
  VISIT(referrer);
  VISIT(title);
  VISIT_ENUM(page_transition);
  VISIT_ENUM(redirect_type);
  VISIT(unique_id);
  VISIT(timestamp_msec);
  VISIT(navigation_forward_back);
  VISIT(navigation_from_address_bar);
  VISIT(navigation_home_page);
  VISIT(global_id);
  VISIT(favicon_url);
  VISIT_ENUM(blocked_state);
  VISIT(http_status_code);
  VISIT(obsolete_referrer_policy);
  VISIT(is_restored);
  VISIT_REP(navigation_redirect);
  VISIT(last_navigation_redirect_url);
  VISIT(correct_referrer_policy);
  VISIT(page_language);
  VISIT_ENUM(password_state);
  VISIT(task_id);
  VISIT_REP(ancestor_task_id);
  VISIT(replaced_navigation);
}

VISIT_PROTO_FIELDS(const sync_pb::ThemeSpecifics& proto) {
  VISIT(use_custom_theme);
  VISIT(use_system_theme_by_default);
  VISIT(custom_theme_name);
  VISIT(custom_theme_id);
  VISIT(custom_theme_update_url);
  VISIT(autogenerated_color_theme);
  VISIT(user_color_theme);
  VISIT(grayscale_theme_enabled);
  VISIT_ENUM(browser_color_scheme);
  VISIT(ntp_background);
}

VISIT_PROTO_FIELDS(
    const sync_pb::ThemeSpecifics::AutogeneratedColorTheme& proto) {
  VISIT(color);
}

VISIT_PROTO_FIELDS(const sync_pb::ThemeSpecifics::UserColorTheme& proto) {
  VISIT(color);
  VISIT_ENUM(browser_color_variant);
}

VISIT_PROTO_FIELDS(const sync_pb::ThemeSpecifics::Empty& proto) {}

VISIT_PROTO_FIELDS(const sync_pb::ThemeSpecifics::NtpCustomBackground& proto) {
  VISIT(url);
  VISIT(attribution_line_1);
  VISIT(attribution_line_2);
  VISIT(attribution_action_url);
  VISIT(collection_id);
  VISIT(resume_token);
  VISIT(refresh_timestamp_unix_epoch_seconds);
  VISIT(main_color);
}

VISIT_PROTO_FIELDS(const sync_pb::TimeRangeDirective& proto) {
  VISIT(start_time_usec);
  VISIT(end_time_usec);
  VISIT(app_id);
}

VISIT_PROTO_FIELDS(const sync_pb::UrlDirective& proto) {
  VISIT(url);
  VISIT(end_time_usec);
  VISIT(app_id);
}

VISIT_PROTO_FIELDS(const sync_pb::TypeHint& proto) {
  VISIT(data_type_id);
  VISIT(has_valid_hint);
}

VISIT_PROTO_FIELDS(const sync_pb::TypedUrlSpecifics& proto) {
  VISIT(url);
  VISIT(title);
  VISIT(hidden);
  VISIT_REP(visits);
  VISIT_REP(visit_transitions);
}

VISIT_PROTO_FIELDS(const sync_pb::UnencryptedSharingMessage& proto) {
  VISIT(sender_guid);
  VISIT(sender_device_name);
}

VISIT_PROTO_FIELDS(const sync_pb::UniquePosition& proto) {
  VISIT_BYTES(value);
  VISIT_BYTES(compressed_value);
  VISIT(uncompressed_length);
  VISIT_BYTES(custom_compressed_v1);
}

VISIT_PROTO_FIELDS(const sync_pb::UserConsentSpecifics& proto) {
  VISIT(locale);
  VISIT(client_consent_time_usec);
  VISIT(account_id);
  VISIT(sync_consent);
  VISIT(arc_backup_and_restore_consent);
  VISIT(arc_location_service_consent);
  VISIT(arc_play_terms_of_service_consent);
  VISIT(assistant_activity_control_consent);
  VISIT(account_passwords_consent);
  VISIT(recorder_speaker_label_consent);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent& proto) {
  VISIT_REP(description_grd_ids);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent& proto) {
  VISIT_REP(description_grd_ids);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent& proto) {
  VISIT(play_terms_of_service_text_length);
  VISIT(play_terms_of_service_hash);
  VISIT(confirmation_grd_id);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserConsentTypes::AssistantActivityControlConsent& proto) {
  VISIT(ui_audit_key);
  VISIT_ENUM(status);
  VISIT_ENUM(setting_type);
}

VISIT_PROTO_FIELDS(const sync_pb::UserConsentTypes::SyncConsent& proto) {
  VISIT_REP(description_grd_ids);
  VISIT(confirmation_grd_id);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(const sync_pb::UserConsentTypes::UnifiedConsent& proto) {
  VISIT_REP(description_grd_ids);
  VISIT(confirmation_grd_id);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserConsentTypes::AccountPasswordsConsent& proto) {
  VISIT_REP(description_grd_ids);
  VISIT(confirmation_grd_id);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent& proto) {
  VISIT_REP(description_grd_ids);
  VISIT(confirmation_grd_id);
  VISIT_ENUM(status);
}

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics& proto) {
  VISIT(event_time_usec);
  VISIT(navigation_id);
  VISIT(session_id);
  VISIT(test_event);
  VISIT(gaia_password_reuse_event);
  VISIT(gaia_password_captured_event);
  VISIT(floc_id_computed_event);
}

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics::Test& proto) {}

VISIT_PROTO_FIELDS(const sync_pb::CloudTokenData& proto) {
  VISIT(suffix);
  VISIT(exp_month);
  VISIT(exp_year);
  VISIT(art_fife_url);
  VISIT(instrument_token);
}

VISIT_PROTO_FIELDS(const sync_pb::PaymentInstrument& proto) {
  VISIT_REP(supported_rails);
  VISIT(display_icon_url);
  VISIT(instrument_id);
  VISIT(bank_account);
  VISIT(nickname);
  VISIT(iban);
  VISIT(ewallet_details);
  VISIT(device_details);
}

VISIT_PROTO_FIELDS(const sync_pb::BankAccountDetails& proto) {
  VISIT(bank_name);
  VISIT(account_number_suffix);
  VISIT_ENUM(account_type);
}

VISIT_PROTO_FIELDS(const sync_pb::EwalletDetails& proto) {
  VISIT(ewallet_name);
  VISIT(account_display_name);
  VISIT_REP(supported_payment_link_uris);
}

VISIT_PROTO_FIELDS(const sync_pb::DeviceDetails& proto) {
  VISIT(is_fido_enrolled);
}

VISIT_PROTO_FIELDS(const sync_pb::CardBenefit& proto) {
  VISIT(benefit_id);
  VISIT(benefit_description);
  VISIT(start_time_unix_epoch_milliseconds);
  VISIT(end_time_unix_epoch_milliseconds);
  VISIT(flat_rate_benefit);
  VISIT(category_benefit);
  VISIT(merchant_benefit);
}

VISIT_PROTO_FIELDS(const sync_pb::CardBenefit_FlatRateBenefit& proto) {}

VISIT_PROTO_FIELDS(const sync_pb::CardBenefit_CategoryBenefit& proto) {
  VISIT_ENUM(category_benefit_type);
}

VISIT_PROTO_FIELDS(const sync_pb::CardBenefit_MerchantBenefit& proto) {
  VISIT_REP(merchant_domain);
}

VISIT_PROTO_FIELDS(const sync_pb::CardIssuer& proto) {
  VISIT_ENUM(issuer);
  VISIT(issuer_id);
}

VISIT_PROTO_FIELDS(const sync_pb::WalletMaskedCreditCard& proto) {
  VISIT(id);
  VISIT_ENUM(status);
  VISIT(name_on_card);
  VISIT_ENUM(type);
  VISIT(last_four);
  VISIT(exp_month);
  VISIT(exp_year);
  VISIT(billing_address_id);
  VISIT(bank_name);
  VISIT(nickname);
  VISIT(card_issuer);
  VISIT(instrument_id);
  VISIT_ENUM(virtual_card_enrollment_state);
  VISIT(card_art_url);
  VISIT(product_description);
  VISIT_ENUM(virtual_card_enrollment_type);
  VISIT_REP(card_benefit);
  VISIT(product_terms_url);
  VISIT_ENUM(card_info_retrieval_enrollment_state);
}

VISIT_PROTO_FIELDS(const sync_pb::WalletMetadataSpecifics& proto) {
  VISIT_ENUM(type);
  VISIT(id);
  VISIT(use_count);
  VISIT(use_date);
  VISIT(card_billing_address_id);
  VISIT(address_has_converted);
}

VISIT_PROTO_FIELDS(const sync_pb::WalletPostalAddress& proto) {
  VISIT(id);
  VISIT(recipient_name);
  VISIT(company_name);
  VISIT_REP(street_address);
  VISIT(address_1);
  VISIT(address_2);
  VISIT(address_3);
  VISIT(address_4);
  VISIT(postal_code);
  VISIT(sorting_code);
  VISIT(country_code);
  VISIT(phone_number);
  VISIT(language_code);
}

VISIT_PROTO_FIELDS(const sync_pb::PaymentsCustomerData& proto) {
  VISIT(id);
}

VISIT_PROTO_FIELDS(const sync_pb::WalletCreditCardCloudTokenData& proto) {
  VISIT(masked_card_id);
  VISIT(suffix);
  VISIT(exp_month);
  VISIT(exp_year);
  VISIT(art_fife_url);
  VISIT(instrument_token);
}

VISIT_PROTO_FIELDS(const sync_pb::WalletMaskedIban& proto) {
  VISIT(instrument_id);
  VISIT(prefix);
  VISIT(suffix);
  VISIT(length);
  VISIT(nickname);
}

VISIT_PROTO_FIELDS(const sync_pb::WebApkIconInfo& proto) {
  VISIT(size_in_px);
  VISIT(url);
  VISIT_ENUM(purpose);
}

VISIT_PROTO_FIELDS(const sync_pb::WebApkSpecifics& proto) {
  VISIT(manifest_id);
  VISIT(start_url);
  VISIT(name);
  VISIT(theme_color);
  VISIT(scope);
  VISIT_REP(icon_infos);
  VISIT(last_used_time_windows_epoch_micros);
}

VISIT_PROTO_FIELDS(const sync_pb::WebAppIconInfo& proto) {
  VISIT(size_in_px);
  VISIT(url);
  VISIT_ENUM(purpose);
}

VISIT_PROTO_FIELDS(const sync_pb::WebAppSpecifics& proto) {
  VISIT(start_url);
  VISIT(name);
  VISIT_ENUM(user_display_mode_default);
  VISIT(theme_color);
  VISIT(scope);
  VISIT_REP(icon_infos);
  VISIT(user_page_ordinal);
  VISIT(user_launch_ordinal);
  VISIT(relative_manifest_id);
  VISIT_ENUM(user_display_mode_cros);
}

VISIT_PROTO_FIELDS(const sync_pb::WifiConfigurationSpecifics::
                       ProxyConfiguration::ManualProxyConfiguration& proto) {
  VISIT(http_proxy_url);
  VISIT(http_proxy_port);
  VISIT(secure_http_proxy_url);
  VISIT(secure_http_proxy_port);
  VISIT(socks_host_url);
  VISIT(socks_host_port);
  VISIT_REP(excluded_domains);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WifiConfigurationSpecifics::ProxyConfiguration& proto) {
  VISIT_ENUM(proxy_option);
  VISIT(autoconfiguration_url);
  VISIT(manual_proxy_configuration);
}

VISIT_PROTO_FIELDS(const sync_pb::WifiConfigurationSpecifics& proto) {
  VISIT_BYTES(hex_ssid);
  VISIT_ENUM(security_type);
  VISIT_BYTES(passphrase);
  VISIT_ENUM(automatically_connect);
  VISIT_ENUM(is_preferred);
  VISIT(proxy_configuration);
  VISIT_REP(custom_dns);
  VISIT(last_connected_timestamp);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics& proto) {
  VISIT(uuid);
  VISIT(name);
  VISIT(created_time_windows_epoch_micros);
  VISIT(updated_time_windows_epoch_micros);
  VISIT(desk);
  VISIT_ENUM(desk_type);
  VISIT(client_cache_guid);
  VISIT_ENUM(device_form_factor);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics::App& proto) {
  VISIT(window_bound);
  VISIT_ENUM(window_state);
  VISIT(z_index);
  VISIT(app);
  VISIT(window_id);
  VISIT(display_id);
  VISIT_ENUM(pre_minimized_window_state);
  VISIT(snap_percentage);
  VISIT_ENUM(container);
  VISIT_ENUM(disposition);
  VISIT(app_name);
  VISIT(title);
  VISIT(override_url);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics::AppOneOf& proto) {
  VISIT(browser_app_window);
  VISIT(chrome_app);
  VISIT(progress_web_app);
  VISIT(arc_app);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WorkspaceDeskSpecifics::BrowserAppWindow& proto) {
  VISIT_REP(tabs);
  VISIT(active_tab_index);
  VISIT(show_as_app);
  VISIT_REP(tab_groups);
  VISIT(first_non_pinned_tab_index);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WorkspaceDeskSpecifics::BrowserAppWindow::BrowserAppTab&
        proto) {
  VISIT(url);
  VISIT(title);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics::ChromeApp& proto) {
  VISIT(app_id);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics::Desk& proto) {
  VISIT_REP(apps);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WorkspaceDeskSpecifics::ProgressiveWebApp& proto) {
  VISIT(app_id);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics::WindowBound& proto) {
  VISIT(top);
  VISIT(left);
  VISIT(width);
  VISIT(height);
}

VISIT_PROTO_FIELDS(const sync_pb::WorkspaceDeskSpecifics::ArcApp& proto) {
  VISIT(app_id);
  VISIT(minimum_size);
  VISIT(maximum_size);
  VISIT(bounds_in_root);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WorkspaceDeskSpecifics::ArcApp::WindowSize& proto) {
  VISIT(width);
  VISIT(height);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WorkspaceDeskSpecifics::BrowserAppWindow::TabGroup& proto) {
  VISIT(first_index);
  VISIT(last_index);
  VISIT(title);
  VISIT_ENUM(color);
  VISIT(is_collapsed);
}

}  // namespace syncer

#undef VISIT_
#undef VISIT_BYTES
#undef VISIT_ENUM
#undef VISIT
#undef VISIT_REP

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_VISITORS_H_
