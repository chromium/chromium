// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service_impl.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/personal_context/core/network/personal_context_manager.h"
#include "components/personal_context/core/personal_context_key_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace personal_context {

namespace {

constexpr std::string_view kUnspecified = "UNSPECIFIED";

bool IsSpecified(std::string_view str) {
  std::string_view trimmed = base::TrimWhitespaceASCII(str, base::TRIM_ALL);
  return !trimmed.empty() &&
         !base::EqualsCaseInsensitiveASCII(trimmed, kUnspecified);
}

void RecordDecryptionMetrics(PersonalContextDecryptionStatus status,
                             base::TimeDelta latency) {
  base::UmaHistogramEnumeration("PersonalContext.DecryptEntity.Status", status);
  base::UmaHistogramBoolean("PersonalContext.DecryptEntity.Result",
                            status == PersonalContextDecryptionStatus::kSuccess);
  base::UmaHistogramMicrosecondsTimes("PersonalContext.DecryptEntity.Latency",
                                      latency);
}

std::optional<proto::SourceReference> ToChromeSourceReference(
    const proto::DecryptedReference& decrypted_ref) {
  switch (decrypted_ref.reference_case()) {
    case proto::DecryptedReference::kGmailMessage: {
      const proto::DecryptedGmailMessage& gmail_msg =
          decrypted_ref.gmail_message();
      if (!IsSpecified(gmail_msg.message_url())) {
        return std::nullopt;
      }
      proto::SourceReference source_ref;
      proto::GmailReference* gmail = source_ref.mutable_gmail();
      gmail->set_message_url(gmail_msg.message_url());
      if (IsSpecified(gmail_msg.subject())) {
        gmail->set_subject(gmail_msg.subject());
      }
      return source_ref;
    }
    case proto::DecryptedReference::kPhoto: {
      proto::SourceReference source_ref;
      source_ref.mutable_photos()->set_photos_url(
          decrypted_ref.photo().deeplink_url());
      return source_ref;
    }
    case proto::DecryptedReference::kVideo: {
      proto::SourceReference source_ref;
      source_ref.mutable_photos()->set_photos_url(
          decrypted_ref.video().deeplink_url());
      return source_ref;
    }
    case proto::DecryptedReference::kPhotosAlbum: {
      proto::SourceReference source_ref;
      source_ref.mutable_photos()->set_photos_url(
          decrypted_ref.photos_album().deeplink_url());
      return source_ref;
    }
    case proto::DecryptedReference::kDriveFile: {
      const proto::DecryptedDriveFile& drive_file = decrypted_ref.drive_file();
      proto::SourceReference source_ref;
      proto::DriveFile* drive = source_ref.mutable_drive();
      if (IsSpecified(drive_file.name())) {
        drive->set_name(drive_file.name());
      }
      if (IsSpecified(drive_file.url())) {
        drive->set_url(drive_file.url());
      }
      return source_ref;
    }
    case proto::DecryptedReference::REFERENCE_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}

std::vector<proto::SourceReference> ExtractSourceReferences(
    const proto::DecryptedEntity& decrypted_entity) {
  std::vector<proto::SourceReference> source_references;
  absl::flat_hash_set<std::string> seen_references;
  for (const auto& decrypted_ref : decrypted_entity.references()) {
    std::optional<proto::SourceReference> source_ref =
        ToChromeSourceReference(decrypted_ref);
    if (!source_ref.has_value()) {
      continue;
    }
    const std::string serialized = source_ref->SerializeAsString();
    if (seen_references.insert(serialized).second) {
      source_references.push_back(std::move(*source_ref));
    }
  }
  return source_references;
}

proto::Entity ExtractPassport(const proto::DecryptedEntity& decrypted_entity) {
  CHECK_EQ(decrypted_entity.entity_case(), proto::DecryptedEntity::kPassport);
  const proto::DecryptedPassport& decrypted_passport =
      decrypted_entity.passport();
  proto::Entity entity;
  proto::Passport* passport = entity.mutable_passport();
  if (IsSpecified(decrypted_passport.full_name())) {
    passport->set_name(decrypted_passport.full_name());
  }
  if (IsSpecified(decrypted_passport.number())) {
    passport->set_number(decrypted_passport.number());
  }
  if (decrypted_passport.has_expiration_date()) {
    *passport->mutable_expiration_date() =
        decrypted_passport.expiration_date();
  }
  if (decrypted_passport.has_issue_date()) {
    *passport->mutable_issue_date() = decrypted_passport.issue_date();
  }
  if (IsSpecified(decrypted_passport.issuing_country())) {
    passport->set_issuing_country(decrypted_passport.issuing_country());
  }
  std::vector<proto::SourceReference> source_references =
      ExtractSourceReferences(decrypted_entity);
  for (auto& ref : source_references) {
    *entity.add_source_references() = std::move(ref);
  }
  return entity;
}

proto::Entity ExtractDriversLicense(
    const proto::DecryptedEntity& decrypted_entity) {
  CHECK_EQ(decrypted_entity.entity_case(),
           proto::DecryptedEntity::kDriversLicense);
  const proto::DecryptedDriversLicense& decrypted_dl =
      decrypted_entity.drivers_license();
  proto::Entity entity;
  proto::DriversLicense* drivers_license = entity.mutable_drivers_license();
  if (IsSpecified(decrypted_dl.full_name())) {
    drivers_license->set_name(decrypted_dl.full_name());
  }
  if (IsSpecified(decrypted_dl.number())) {
    drivers_license->set_number(decrypted_dl.number());
  }
  if (decrypted_dl.has_expiration_date()) {
    *drivers_license->mutable_expiration_date() =
        decrypted_dl.expiration_date();
  }
  if (decrypted_dl.has_issue_date()) {
    *drivers_license->mutable_issue_date() = decrypted_dl.issue_date();
  }
  if (IsSpecified(decrypted_dl.issuing_region())) {
    drivers_license->set_state(decrypted_dl.issuing_region());
  }
  std::vector<proto::SourceReference> source_references =
      ExtractSourceReferences(decrypted_entity);
  for (auto& ref : source_references) {
    *entity.add_source_references() = std::move(ref);
  }
  return entity;
}

std::optional<proto::Entity> DecryptedEntityToChromeEntity(
    const proto::DecryptedEntity& decrypted_entity) {
  switch (decrypted_entity.entity_case()) {
    case proto::DecryptedEntity::kPassport:
      return ExtractPassport(decrypted_entity);
    case proto::DecryptedEntity::kDriversLicense:
      return ExtractDriversLicense(decrypted_entity);
    case proto::DecryptedEntity::ENTITY_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

PersonalContextServiceImpl::PersonalContextServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : personal_context_manager_(std::make_unique<PersonalContextManager>(
          std::move(url_loader_factory),
          identity_manager)),
      key_manager_(pref_service ? std::make_unique<PersonalContextKeyManager>(
                                      pref_service,
                                      device_info_sync_service)
                                : nullptr) {}

PersonalContextServiceImpl::~PersonalContextServiceImpl() = default;

void PersonalContextServiceImpl::Shutdown() {
  personal_context_manager_->Shutdown();
}

void PersonalContextServiceImpl::FetchContext(
    proto::ContextMemoryFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    const ContextMemoryRequestOptions& options,
    FetchContextCallback callback) {
  personal_context_manager_->FetchContext(
      feature, request_metadata, options.request_timeout, std::move(callback));
}

void PersonalContextServiceImpl::FetchPiiEntities(
    const proto::FetchPiiEntitiesRequest& request,
    const ContextMemoryRequestOptions& options,
    FetchPiiContextCallback callback) {
  personal_context_manager_->FetchPiiEntities(request, options.request_timeout,
                                              std::move(callback));
}

std::optional<proto::Entity> PersonalContextServiceImpl::DecryptEntity(
    const proto::Entity& entity) {
  CHECK(entity.entity_case() == proto::Entity::kEncryptedEntity);
  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (!key_manager_) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kNoKeyManager,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  if (entity.encrypted_entity().empty()) {
    RecordDecryptionMetrics(
        PersonalContextDecryptionStatus::kEmptyEncryptedEntity,
        base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> decrypted_bytes =
      key_manager_->Open(base::as_byte_span(entity.encrypted_entity()));
  if (!decrypted_bytes.has_value()) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kDecryptionFailed,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  proto::DecryptedEntity decrypted_entity;
  if (!decrypted_entity.ParseFromArray(decrypted_bytes->data(),
                                       decrypted_bytes->size())) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kProtoParseFailed,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  std::optional<proto::Entity> converted_entity =
      DecryptedEntityToChromeEntity(decrypted_entity);
  if (!converted_entity.has_value()) {
    RecordDecryptionMetrics(PersonalContextDecryptionStatus::kProtoParseFailed,
                            base::TimeTicks::Now() - start_time);
    return std::nullopt;
  }

  RecordDecryptionMetrics(PersonalContextDecryptionStatus::kSuccess,
                          base::TimeTicks::Now() - start_time);
  return converted_entity;
}

}  // namespace personal_context
