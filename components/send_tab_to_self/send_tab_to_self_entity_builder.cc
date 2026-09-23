// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entity_builder.h"

#include <utility>

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/model/crypto/key_derivation_params.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/fake_server.h"

namespace send_tab_to_self {

namespace {

constexpr char kClientEntityNonUniqueName[] = "non_unique_name";

// Populates `form_fields` and `text_fragment` data into the `PageContext`
// of `specifics`, and performs Nigori encryption if `fake_server` is
// provided and has keystore keys.
void PopulatePageContext(
    const std::vector<SendTabToSelfEntityBuilder::FormField>& form_fields,
    const std::string& text_fragment,
    fake_server::FakeServer* fake_server,
    sync_pb::SendTabToSelfSpecifics* specifics) {
  if (form_fields.empty() && text_fragment.empty()) {
    return;
  }

  PageContext context;
  for (const auto& field_data : form_fields) {
    PageContext::FormField field;
    field.id_attribute = base::UTF8ToUTF16(field_data.id_attribute);
    field.name_attribute = base::UTF8ToUTF16(field_data.name_attribute);
    field.value = base::UTF8ToUTF16(field_data.value);
    field.form_control_type = autofill::FormControlType::kInputText;
    context.form_field_info.fields.push_back(std::move(field));
  }

  if (!text_fragment.empty()) {
    std::optional<shared_highlighting::TextFragment> parsed_fragment =
        shared_highlighting::TextFragment::FromEscapedString(text_fragment);
    if (parsed_fragment) {
      context.scroll_position.text_fragment =
          TextFragmentData(*parsed_fragment);
    }
  }

  *specifics->mutable_page_context() = PageContextToProto(context);

  if (!fake_server) {
    return;
  }

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      fake_server->GetKeystoreKeys();
  if (keystore_keys.empty()) {
    return;
  }

  std::string key_base64 = base::Base64Encode(keystore_keys.back());
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(  // IN-TEST
          key_base64, syncer::KeyDerivationParams::CreateForPbkdf2());
  CHECK(cryptographer);
  CHECK(cryptographer->Encrypt(specifics->page_context(),
                               specifics->mutable_encrypted_page_context()));
  specifics->clear_page_context();
}

}  // namespace

SendTabToSelfEntityBuilder::SendTabToSelfEntityBuilder(GURL url,
                                                       std::string title)
    : url_(std::move(url)),
      title_(std::move(title)),
      guid_(base::Uuid::GenerateRandomV4().AsLowercaseString()) {}

SendTabToSelfEntityBuilder::~SendTabToSelfEntityBuilder() = default;

SendTabToSelfEntityBuilder::SendTabToSelfEntityBuilder(
    SendTabToSelfEntityBuilder&&) = default;

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::operator=(
    SendTabToSelfEntityBuilder&&) = default;

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::SetTitle(
    std::string title) {
  title_ = std::move(title);
  return *this;
}

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::SetGuid(
    std::string guid) {
  guid_ = std::move(guid);
  return *this;
}

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::SetDeviceName(
    std::string device_name) {
  device_name_ = std::move(device_name);
  return *this;
}

SendTabToSelfEntityBuilder&
SendTabToSelfEntityBuilder::SetTargetDeviceCacheGuid(std::string target_guid) {
  target_device_cache_guid_ = std::move(target_guid);
  return *this;
}

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::SetSharedTime(
    base::Time time) {
  shared_time_ = time;
  return *this;
}

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::SetOpened(bool opened) {
  opened_ = opened;
  return *this;
}

SendTabToSelfEntityBuilder&
SendTabToSelfEntityBuilder::SetNotificationDismissed(bool dismissed) {
  notification_dismissed_ = dismissed;
  return *this;
}

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::AddFormField(
    std::string_view id_attribute,
    std::string_view value,
    std::string_view name_attribute) {
  form_fields_.push_back(FormField{
      .id_attribute = std::string(id_attribute),
      .name_attribute = std::string(name_attribute),
      .value = std::string(value),
  });
  return *this;
}

SendTabToSelfEntityBuilder& SendTabToSelfEntityBuilder::SetTextFragment(
    std::string_view text_fragment) {
  text_fragment_ = std::string(text_fragment);
  return *this;
}

std::unique_ptr<syncer::LoopbackServerEntity> SendTabToSelfEntityBuilder::Build(
    fake_server::FakeServer* fake_server) const {
  base::Time shared_time =
      shared_time_.is_null() ? base::Time::Now() : shared_time_;
  int64_t shared_time_usec =
      shared_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::SendTabToSelfSpecifics* specifics =
      entity_specifics.mutable_send_tab_to_self();

  specifics->set_guid(guid_);
  specifics->set_url(url_.spec());
  specifics->set_device_name(device_name_);
  specifics->set_target_device_sync_cache_guid(target_device_cache_guid_);
  specifics->set_shared_time_usec(shared_time_usec);
  specifics->set_title(title_);
  specifics->set_opened(opened_);
  specifics->set_notification_dismissed(notification_dismissed_);

  PopulatePageContext(form_fields_, text_fragment_, fake_server, specifics);

  return syncer::PersistentUniqueClientEntity::
      CreateFromSpecificsForTesting(  // IN-TEST
          kClientEntityNonUniqueName, guid_, entity_specifics,
          /*creation_time=*/shared_time_usec,
          /*last_modified_time=*/shared_time_usec);
}

}  // namespace send_tab_to_self
