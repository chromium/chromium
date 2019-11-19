// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_VISITORS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_VISITORS_H_

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/dictionary_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/experiments_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/favicon_image_specifics.pb.h"
#include "components/sync/protocol/favicon_tracking_specifics.pb.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/os_preference_specifics.pb.h"
#include "components/sync/protocol/os_priority_preference_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/persisted_entity_data.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/printer_specifics.pb.h"
#include "components/sync/protocol/priority_preference_specifics.pb.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

// This file implements VisitProtoFields() functions for sync protos.
//
// VisitProtoFields(visitor, proto) calls |visitor| for each field in
// |proto|. When called, |visitor| gets passed |proto|, field name and
// field value.
//
// VisitProtoFields() used to implement two distinctive features:
// 1. Serialization into base::DictionaryValue
// 2. Proto memory usage estimation
//
// To achieve that it's very important for VisitProtoFields() to be free
// of any logic. It must just call visitor for each field in a proto.
//
// Logic (like clobbering sensitive fields) must be implemented in visitors.
// For example see how ToValueVisitor (from proto_value_conversions.cc)
// implements various customizations.

#define VISIT_(Kind, field) \
  if (proto.has_##field()) \
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
#define VISIT_REP(field) \
  visitor.Visit(proto, #field, proto.field());

// NOLINT(runtime/references) is necessary to avoid a presubmit warning about
// V& not being const.
#define VISIT_PROTO_FIELDS(proto) \
  template <class V>              \
  void VisitProtoFields(V& visitor, proto)  // NOLINT(runtime/references)

namespace syncer {

VISIT_PROTO_FIELDS(const sync_pb::AppListSpecifics& proto) {
  VISIT(item_id);
  VISIT_ENUM(item_type);
  VISIT(item_name);
  VISIT(parent_id);
  VISIT(item_ordinal);
  VISIT(item_pin_ordinal);
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
  VISIT(bookmark_app_url);
  VISIT(bookmark_app_description);
  VISIT(bookmark_app_icon_color);
  VISIT_REP(linked_app_icons);
  VISIT(bookmark_app_scope);
  VISIT(bookmark_app_theme_color);
}

VISIT_PROTO_FIELDS(const sync_pb::ArcPackageSpecifics& proto) {
  VISIT(package_name);
  VISIT(package_version);
  VISIT(last_backup_android_id);
  VISIT(last_backup_time);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillCullingFlags& proto) {
  VISIT(enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::AutofillProfileSpecifics& proto) {
  VISIT(guid);
  VISIT(origin);
  VISIT(use_count);
  VISIT(use_date);
  VISIT_REP(name_first);
  VISIT_REP(name_middle);
  VISIT_REP(name_last);
  VISIT_REP(name_full);
  VISIT_REP(email_address);
  VISIT(company_name);
  VISIT(address_home_line1);
  VISIT(address_home_line2);
  VISIT(address_home_city);
  VISIT(address_home_state);
  VISIT(address_home_zip);
  VISIT(address_home_country);
  VISIT(address_home_street_address);
  VISIT(address_home_sorting_code);
  VISIT(address_home_dependent_locality);
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

VISIT_PROTO_FIELDS(const sync_pb::AutofillWalletSpecifics& proto) {
  VISIT_ENUM(type);
  VISIT(masked_card);
  VISIT(address);
  VISIT(customer_data);
  VISIT(cloud_token_data);
}

VISIT_PROTO_FIELDS(const sync_pb::BookmarkSpecifics& proto) {
  VISIT(url);
  VISIT_BYTES(favicon);
  VISIT(guid);
  VISIT(title);
  VISIT(creation_time_us);
  VISIT(icon_url);
  VISIT_REP(meta_info);
}

VISIT_PROTO_FIELDS(const sync_pb::ChromiumExtensionsActivity& proto) {
  VISIT(extension_id);
  VISIT(bookmark_writes_since_last_commit);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientCommand& proto) {
  VISIT(set_sync_poll_interval);
  VISIT(max_commit_batch_size);
  VISIT(sessions_commit_delay_seconds);
  VISIT(throttle_delay_seconds);
  VISIT(client_invalidation_hint_buffer_size);
}

VISIT_PROTO_FIELDS(const sync_pb::ClientConfigParams& proto) {
  VISIT_REP(enabled_type_ids);
  VISIT(tabs_datatype_enabled);
  VISIT(cookie_jar_mismatch);
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
  VISIT(sync_problem_detected);
  VISIT(debug_info);
  VISIT(client_status);
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
  VISIT(url);
  VISIT_ENUM(action);
}

VISIT_PROTO_FIELDS(const sync_pb::CommitMessage& proto) {
  VISIT_REP(entries);
  VISIT(cache_guid);
  VISIT_REP(extensions_activity);
  VISIT(config_params);
}

VISIT_PROTO_FIELDS(const sync_pb::CommitResponse& proto) {
  VISIT_REP(entryresponse);
}

VISIT_PROTO_FIELDS(const sync_pb::CommitResponse::EntryResponse& proto) {
  VISIT_ENUM(response_type);
  VISIT(id_string);
  VISIT(parent_id_string);
  VISIT(version);
  VISIT(name);
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
  VISIT(timestamp_token_for_migration);
  VISIT(notification_hint);
  VISIT(get_update_triggers);
}

VISIT_PROTO_FIELDS(const sync_pb::DatatypeAssociationStats& proto) {
  VISIT(data_type_id);
  VISIT(num_local_items_before_association);
  VISIT(num_sync_items_before_association);
  VISIT(num_local_items_after_association);
  VISIT(num_sync_items_after_association);
  VISIT(num_local_items_added);
  VISIT(num_local_items_deleted);
  VISIT(num_local_items_modified);
  VISIT(num_sync_items_added);
  VISIT(num_sync_items_deleted);
  VISIT(num_sync_items_modified);
  VISIT(local_version_pre_association);
  VISIT(sync_version_pre_association);
  VISIT(had_error);
  VISIT(download_wait_time_us);
  VISIT(download_time_us);
  VISIT(association_wait_time_for_high_priority_us);
  VISIT(association_wait_time_for_same_priority_us);
  VISIT_REP(high_priority_type_configured_before);
  VISIT_REP(same_priority_type_configured_before);
}

VISIT_PROTO_FIELDS(const sync_pb::DebugEventInfo& proto) {
  VISIT_ENUM(singleton_event);
  VISIT(sync_cycle_completed_event_info);
  VISIT(nudging_datatype);
  VISIT_REP(datatypes_notified_from_server);
  VISIT(datatype_association_stats);
}

VISIT_PROTO_FIELDS(const sync_pb::DebugInfo& proto) {
  VISIT_REP(events);
  VISIT(cryptographer_ready);
  VISIT(cryptographer_has_pending_keys);
  VISIT(events_dropped);
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
}

VISIT_PROTO_FIELDS(const sync_pb::FeatureSpecificFields& proto) {
  VISIT(send_tab_to_self_receiving_enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::SharingSpecificFields& proto) {
  VISIT(vapid_fcm_token);
  VISIT_BYTES(vapid_p256dh);
  VISIT_BYTES(vapid_auth_secret);
  VISIT_REP(enabled_features);
  VISIT(sender_id_fcm_token);
  VISIT_BYTES(sender_id_p256dh);
  VISIT_BYTES(sender_id_auth_secret);
}

VISIT_PROTO_FIELDS(const sync_pb::DictionarySpecifics& proto) {
  VISIT(word);
}

VISIT_PROTO_FIELDS(const sync_pb::EncryptedData& proto) {
  VISIT(key_name);
  // TODO(akalin): Shouldn't blob be of type bytes instead of string?
  VISIT_BYTES(blob);
}

VISIT_PROTO_FIELDS(const sync_pb::EnhancedBookmarksFlags& proto) {
  VISIT(enabled);
  VISIT(extension_id);
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
}

VISIT_PROTO_FIELDS(const sync_pb::EntitySpecifics& proto) {
  static_assert(40 == ModelType::NUM_ENTRIES,
                "When adding a new protocol type, you will likely need to add "
                "it here as well.");
  VISIT(encrypted);
  VISIT(app);
  VISIT(app_list);
  VISIT(app_setting);
  VISIT(arc_package);
  VISIT(autofill);
  VISIT(autofill_profile);
  VISIT(autofill_wallet);
  VISIT(bookmark);
  VISIT(device_info);
  VISIT(dictionary);
  VISIT(experiments);
  VISIT(extension);
  VISIT(extension_setting);
  VISIT(favicon_image);
  VISIT(favicon_tracking);
  VISIT(history_delete_directive);
  VISIT(managed_user_setting);
  VISIT(managed_user_whitelist);
  VISIT(nigori);
  VISIT(os_preference);
  VISIT(os_priority_preference);
  VISIT(password);
  VISIT(preference);
  VISIT(printer);
  VISIT(priority_preference);
  VISIT(reading_list);
  VISIT(search_engine);
  VISIT(security_event);
  VISIT(send_tab_to_self);
  VISIT(session);
  VISIT(theme);
  VISIT(typed_url);
  VISIT(user_consent);
  VISIT(user_event);
  VISIT(wallet_metadata);
  VISIT(web_app);
  VISIT(wifi_configuration);
}

VISIT_PROTO_FIELDS(const sync_pb::ExperimentsSpecifics& proto) {
  VISIT(keystore_encryption);
  VISIT(history_delete_directives);
  VISIT(autofill_culling);
  VISIT(pre_commit_update_avoidance);
  VISIT(favicon_sync);
  VISIT(gcm_channel);
  VISIT(gcm_invalidations);
  VISIT(obsolete_enhanced_bookmarks);
  VISIT(obsolete_wallet_sync);
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
  VISIT(name);
  VISIT(remote_install);
  VISIT(all_urls_enabled);
  VISIT(disable_reasons);
}

VISIT_PROTO_FIELDS(const sync_pb::FaviconData& proto) {
  VISIT_BYTES(favicon);
  VISIT(width);
  VISIT(height);
}

VISIT_PROTO_FIELDS(const sync_pb::FaviconImageSpecifics& proto) {
  VISIT(favicon_url);
  VISIT(favicon_web);
  VISIT(favicon_web_32);
  VISIT(favicon_touch_64);
  VISIT(favicon_touch_precomposed_64);
}

VISIT_PROTO_FIELDS(const sync_pb::FaviconSyncFlags& proto) {
  VISIT(enabled);
  VISIT(favicon_sync_limit);
}

VISIT_PROTO_FIELDS(const sync_pb::FaviconTrackingSpecifics& proto) {
  VISIT(favicon_url);
  VISIT(last_visit_time_ms);
  VISIT(is_bookmarked);
}

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics::FieldTrial& proto) {
  VISIT_REP(variation_ids);
}

VISIT_PROTO_FIELDS(const sync_pb::GcmChannelFlags& proto) {
  VISIT(enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::GcmInvalidationsFlags& proto) {
  VISIT(enabled);
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
  VISIT(batch_size);
  VISIT_REP(from_progress_marker);
  VISIT(streaming);
  VISIT(need_encryption_key);
  VISIT(create_mobile_bookmarks_folder);
  VISIT_ENUM(get_updates_origin);
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

VISIT_PROTO_FIELDS(const sync_pb::HistoryDeleteDirectives& proto) {
  VISIT(enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::KeystoreEncryptionFlags& proto) {
  VISIT(enabled);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserEventSpecifics::LanguageDetection::Language& proto) {
  VISIT(language_code);
  VISIT(is_reliable);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserEventSpecifics::LanguageDetection& proto) {
  VISIT_REP(detected_languages);
  VISIT(adopted_language_code);
}

VISIT_PROTO_FIELDS(const sync_pb::LinkedAppIconInfo& proto) {
  VISIT(url);
  VISIT(size);
}

VISIT_PROTO_FIELDS(const sync_pb::ManagedUserSettingSpecifics& proto) {
  VISIT(name);
  VISIT(value);
}

VISIT_PROTO_FIELDS(const sync_pb::ManagedUserWhitelistSpecifics& proto) {
  VISIT(id);
  VISIT(name);
}

VISIT_PROTO_FIELDS(const sync_pb::MetaInfo& proto) {
  VISIT(key);
  VISIT(value);
}

VISIT_PROTO_FIELDS(const sync_pb::ModelTypeState& proto) {
  VISIT(progress_marker);
  VISIT(type_context);
  VISIT(encryption_key_name);
  VISIT(initial_sync_done);
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
}

VISIT_PROTO_FIELDS(const sync_pb::NigoriLocalData& proto) {
  VISIT(model_type_state);
  VISIT(entity_metadata);
  VISIT(nigori_model);
}

VISIT_PROTO_FIELDS(const sync_pb::NigoriSpecifics& proto) {
  VISIT(encryption_keybag);
  VISIT(keybag_is_frozen);
  VISIT(encrypt_bookmarks);
  VISIT(encrypt_preferences);
  VISIT(encrypt_autofill_profile);
  VISIT(encrypt_autofill);
  VISIT(encrypt_themes);
  VISIT(encrypt_typed_urls);
  VISIT(encrypt_extension_settings);
  VISIT(encrypt_extensions);
  VISIT(encrypt_sessions);
  VISIT(encrypt_app_settings);
  VISIT(encrypt_apps);
  VISIT(encrypt_search_engines);
  VISIT(encrypt_dictionary);
  VISIT(encrypt_app_list);
  VISIT(encrypt_arc_package);
  VISIT(encrypt_reading_list);
  VISIT(encrypt_everything);
  VISIT(server_only_was_missing_keystore_migration_time);
  VISIT(sync_tab_favicons);
  VISIT(passphrase_type);
  VISIT(keystore_decryptor_token);
  VISIT(keystore_migration_time);
  VISIT(custom_passphrase_time);
  VISIT(custom_passphrase_key_derivation_method);
  VISIT(custom_passphrase_key_derivation_salt);
}

VISIT_PROTO_FIELDS(const sync_pb::OsPreferenceSpecifics& proto) {
  VISIT(preference);
}

VISIT_PROTO_FIELDS(const sync_pb::OsPriorityPreferenceSpecifics& proto) {
  VISIT(preference);
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecifics& proto) {
  VISIT(encrypted);
  VISIT(unencrypted_metadata);
  VISIT(client_only_encrypted_data);
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
}

VISIT_PROTO_FIELDS(const sync_pb::PasswordSpecificsMetadata& proto) {
  VISIT(url);
}

VISIT_PROTO_FIELDS(const sync_pb::PersistedEntityData& proto) {
  VISIT(name);
  VISIT(specifics);
}

VISIT_PROTO_FIELDS(const sync_pb::PreCommitUpdateAvoidanceFlags& proto) {
  VISIT(enabled);
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
}

VISIT_PROTO_FIELDS(const sync_pb::SendTabToSelfSpecifics& proto) {
  VISIT(guid);
  VISIT(title);
  VISIT(url);
  VISIT(shared_time_usec);
  VISIT(navigation_time_usec);
  VISIT(device_name);
  VISIT(target_device_sync_cache_guid);
}

VISIT_PROTO_FIELDS(const sync_pb::SessionHeader& proto) {
  VISIT_REP(window);
  VISIT(client_name);
  VISIT_ENUM(device_type);
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
}

VISIT_PROTO_FIELDS(const sync_pb::SessionWindow& proto) {
  VISIT(window_id);
  VISIT(selected_tab_index);
  VISIT_REP(tab);
  VISIT_ENUM(browser_type);
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
  VISIT(old_parent_id);
  VISIT(version);
  VISIT(mtime);
  VISIT(ctime);
  VISIT(name);
  VISIT(non_unique_name);
  VISIT(server_defined_unique_tag);
  VISIT(position_in_parent);
  VISIT(unique_position);
  VISIT(insert_after_item_id);
  VISIT(deleted);
  VISIT(originator_cache_guid);
  VISIT(originator_client_item_id);
  VISIT(specifics);
  VISIT(folder);
  VISIT(client_defined_unique_tag);
  VISIT_BYTES(ordinal_in_parent);
}

VISIT_PROTO_FIELDS(const sync_pb::SecurityEventSpecifics& proto) {
  VISIT(gaia_password_reuse_event);
  VISIT(event_time_usec);
}

VISIT_PROTO_FIELDS(const sync_pb::GaiaPasswordReuse& proto) {
  VISIT(reuse_detected);
  VISIT(reuse_lookup);
  VISIT(dialog_interaction);
  // TODO(markusheintz): Remove.
  VISIT(password_captured);
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

// TODO(markusheintz): Remove.
VISIT_PROTO_FIELDS(const sync_pb::GaiaPasswordReuse::PasswordCaptured& proto) {
  VISIT_ENUM(event_trigger);
}

VISIT_PROTO_FIELDS(
    const sync_pb::UserEventSpecifics::GaiaPasswordCaptured& proto) {
  VISIT_ENUM(event_trigger);
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
  VISIT(navigation_chain_start);
  VISIT(navigation_chain_end);
  VISIT(global_id);
  VISIT(favicon_url);
  VISIT_ENUM(blocked_state);
  VISIT_REP(content_pack_categories);
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
  VISIT(autogenerated_theme);
}
VISIT_PROTO_FIELDS(const sync_pb::ThemeSpecifics::AutogeneratedTheme& proto) {
  VISIT(color);
}

VISIT_PROTO_FIELDS(const sync_pb::TimeRangeDirective& proto) {
  VISIT(start_time_usec);
  VISIT(end_time_usec);
}

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics::Translation& proto) {
  VISIT(from_language_code);
  VISIT(to_language_code);
  VISIT_ENUM(interaction);
}

VISIT_PROTO_FIELDS(const sync_pb::UrlDirective& proto) {
  VISIT(url);
  VISIT(end_time_usec);
}

// TODO(vitaliii): Delete once UserEventSpecifics::UserConsent is completely
// deprecated.
VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics::UserConsent& proto) {
  VISIT_ENUM(feature);
  VISIT_REP(description_grd_ids);
  VISIT(confirmation_grd_id);
  VISIT(locale);
  VISIT_ENUM(status);
  VISIT(account_id);
  VISIT(sync_consent);
  VISIT(arc_backup_and_restore_consent);
  VISIT(arc_location_service_consent);
  VISIT(arc_play_terms_of_service_consent);
  VISIT(arc_metrics_and_usage_consent);
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

VISIT_PROTO_FIELDS(const sync_pb::UniquePosition& proto) {
  VISIT_BYTES(value);
  VISIT_BYTES(compressed_value);
  VISIT(uncompressed_length);
  VISIT_BYTES(custom_compressed_v1);
}

VISIT_PROTO_FIELDS(const sync_pb::UserConsentSpecifics& proto) {
  VISIT_ENUM(feature);
  VISIT_REP(description_grd_ids);
  VISIT(confirmation_grd_id);
  VISIT(locale);
  VISIT_ENUM(status);
  VISIT(account_id);
  VISIT(sync_consent);
  VISIT(arc_backup_and_restore_consent);
  VISIT(arc_location_service_consent);
  VISIT(arc_play_terms_of_service_consent);
  VISIT(arc_metrics_and_usage_consent);
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
    const sync_pb::UserConsentTypes::ArcMetricsAndUsageConsent& proto) {
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

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics& proto) {
  VISIT(event_time_usec);
  VISIT(navigation_id);
  VISIT(session_id);
  VISIT(test_event);
  VISIT(field_trial_event);
  VISIT(language_detection_event);
  VISIT(translation_event);
  VISIT(user_consent);
  VISIT(gaia_password_reuse_event);
  VISIT(gaia_password_captured_event);
}

VISIT_PROTO_FIELDS(const sync_pb::UserEventSpecifics::Test& proto) {}

VISIT_PROTO_FIELDS(const sync_pb::CloudTokenData& proto) {
  VISIT(suffix);
  VISIT(exp_month);
  VISIT(exp_year);
  VISIT(art_fife_url);
  VISIT(instrument_token);
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
  VISIT_ENUM(card_class);
  VISIT(bank_name);
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

VISIT_PROTO_FIELDS(const sync_pb::WalletSyncFlags& proto) {
  VISIT(enabled);
}

VISIT_PROTO_FIELDS(const sync_pb::WebAppSpecifics& proto) {
  VISIT(launch_url);
  VISIT(name);
  VISIT_ENUM(user_display_mode);
  VISIT(theme_color);
}

VISIT_PROTO_FIELDS(const sync_pb::WifiConfigurationSpecifics& proto) {
  VISIT(encrypted);
  VISIT(client_only_encrypted_data);
}

VISIT_PROTO_FIELDS(
    const sync_pb::WifiConfigurationSpecificsData::ProxyConfiguration& proto) {
  VISIT_ENUM(proxy_option);
  VISIT(proxy_url);
  VISIT(proxy_port);
  VISIT_REP(whitelisted_domains);
}

VISIT_PROTO_FIELDS(const sync_pb::WifiConfigurationSpecificsData& proto) {
  VISIT_BYTES(hex_ssid);
  VISIT_ENUM(security_type);
  VISIT_BYTES(passphrase);
  VISIT_ENUM(automatically_connect);
  VISIT_ENUM(is_preferred);
  VISIT(proxy_configuration);
  VISIT_REP(custom_dns);
  VISIT(last_update_timestamp);
}

}  // namespace syncer

#undef VISIT_
#undef VISIT_BYTES
#undef VISIT_ENUM
#undef VISIT
#undef VISIT_REP

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_VISITORS_H_
