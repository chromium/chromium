// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include "base/strings/string_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

// EncryptedReportingJobConfiguration string
const char EncryptedReportingJobConfiguration::kEncryptedRecordListKey_[] =
    "encryptedRecord";

// EncrypedRecordDictionaryBuilder strings
const char EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::kEncryptedWrappedRecord_[] =
        "encryptedWrappedRecord";
const char EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::kSequencingInformationKey_[] =
        "sequencingInformation";
const char EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::kEncryptionInfoKey_[] = "encryptionInfo";

// SequencingInformationDictionaryBuilder strings
const char EncryptedReportingJobConfiguration::
    SequencingInformationDictionaryBuilder::kSequencingId_[] = "sequencingId";
const char EncryptedReportingJobConfiguration::
    SequencingInformationDictionaryBuilder::kGenerationId_[] = "generationId";
const char EncryptedReportingJobConfiguration::
    SequencingInformationDictionaryBuilder::kPriority_[] = "priority";

// EncryptionInfoDictionaryBuilder strings
const char EncryptedReportingJobConfiguration::EncryptionInfoDictionaryBuilder::
    kEncryptionKey_[] = "encryptionKey";
const char EncryptedReportingJobConfiguration::EncryptionInfoDictionaryBuilder::
    kPublicKeyId_[] = "publicKeyId";

base::Optional<base::Value> EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::ConvertEncryptedRecordProtoToValue(
        const ::reporting::EncryptedRecord& record) {
  // A record without sequencing information cannot be uploaded - deny it.
  if (!record.has_sequencing_information()) {
    return base::nullopt;
  }

  base::Value record_dictionary{base::Value::Type::DICTIONARY};

  auto sequencing_information_result = SequencingInformationDictionaryBuilder::
      ConvertSequencingInformationProtoToValue(record.sequencing_information());
  if (!sequencing_information_result.has_value()) {
    // Sequencing information was improperly configured. Record cannot be
    // uploaded. Deny it.
    return base::nullopt;
  }
  record_dictionary.SetKey(kSequencingInformationKey_,
                           std::move(sequencing_information_result.value()));

  if (record.has_encryption_info()) {
    auto encryption_info_result =
        EncryptionInfoDictionaryBuilder::ConvertEncryptionInfoProtoToValue(
            record.encryption_info());
    if (!encryption_info_result.has_value()) {
      // Encryption info has been corrupted or set improperly. Deny it.
      return base::nullopt;
    }

    record_dictionary.SetKey(kEncryptionInfoKey_,
                             std::move(encryption_info_result.value()));
  }

  // Gap records won't fill in this field, so it can be missing.
  if (record.has_encrypted_wrapped_record()) {
    record_dictionary.SetStringKey(kEncryptedWrappedRecord_,
                                   record.encrypted_wrapped_record());
  }

  return record_dictionary;
}

// static
std::string EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::GetEncryptedWrappedRecordPath() {
  return kEncryptedWrappedRecord_;
}

// static
std::string
EncryptedReportingJobConfiguration::EncryptedRecordDictionaryBuilder::
    GetSequencingInformationSequencingIdPath() {
  return base::JoinString(
      {kSequencingInformationKey_,
       SequencingInformationDictionaryBuilder::GetSequencingIdPath()},
      ".");
}

// static
std::string
EncryptedReportingJobConfiguration::EncryptedRecordDictionaryBuilder::
    GetSequencingInformationGenerationIdPath() {
  return base::JoinString(
      {kSequencingInformationKey_,
       SequencingInformationDictionaryBuilder::GetGenerationIdPath()},
      ".");
}

// static
std::string EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::GetSequencingInformationPriorityPath() {
  return base::JoinString(
      {kSequencingInformationKey_,
       SequencingInformationDictionaryBuilder::GetPriorityPath()},
      ".");
}

// static
std::string EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::GetEncryptionInfoEncryptionKeyPath() {
  return base::JoinString(
      {kEncryptionInfoKey_,
       EncryptionInfoDictionaryBuilder::GetEncryptionKeyPath()},
      ".");
}

// static
std::string EncryptedReportingJobConfiguration::
    EncryptedRecordDictionaryBuilder::GetEncryptionInfoPublicKeyIdPath() {
  return base::JoinString(
      {kEncryptionInfoKey_,
       EncryptionInfoDictionaryBuilder::GetPublicKeyIdPath()},
      ".");
}

EncryptedReportingJobConfiguration::EncryptedReportingJobConfiguration(
    CloudPolicyClient* client,
    const std::string& server_url,
    UploadCompleteCallback callback)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_ENCRYPTED_REPORT,
                                    client->GetURLLoaderFactory(),
                                    client,
                                    server_url,
                                    std::move(callback)) {
  AddEncryptedRecordListToPayload();
}

EncryptedReportingJobConfiguration::~EncryptedReportingJobConfiguration() {
  if (!callback_.is_null()) {
    // The job either wasn't tried, or failed in some unhandled way. Report
    // failure to the callback.
    std::move(callback_).Run(/*job=*/nullptr,
                             DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
                             /*net_error=*/418,
                             /*response_body=*/base::Value());
  }
}

bool EncryptedReportingJobConfiguration::AddEncryptedRecord(
    const ::reporting::EncryptedRecord& record) {
  base::Optional<base::Value> record_result =
      EncryptedRecordDictionaryBuilder::ConvertEncryptedRecordProtoToValue(
          record);
  if (!record_result.has_value()) {
    return false;
  }

  base::Value* encrypted_record_list =
      payload_.FindListKey(kEncryptedRecordListKey_);
  DCHECK(encrypted_record_list);
  encrypted_record_list->Append(std::move(record_result.value()));
  return true;
}

void EncryptedReportingJobConfiguration::UpdateContext(base::Value& context) {
  context_ = std::move(context);
}

// static
std::string EncryptedReportingJobConfiguration::GetEncryptedRecordListKey() {
  return kEncryptedRecordListKey_;
}

std::string EncryptedReportingJobConfiguration::GetUmaString() const {
  return "Enterprise.EncryptedReportingSuccess";
}

void EncryptedReportingJobConfiguration::AddEncryptedRecordListToPayload() {
  payload_.SetKey(kEncryptedRecordListKey_,
                  base::Value{base::Value::Type::LIST});
}

// static
base::Optional<base::Value>
EncryptedReportingJobConfiguration::SequencingInformationDictionaryBuilder::
    ConvertSequencingInformationProtoToValue(
        const ::reporting::SequencingInformation& sequencing_information) {
  // SequencingInformation requires all three fields be set.
  if (!sequencing_information.has_sequencing_id() ||
      !sequencing_information.has_generation_id() ||
      !sequencing_information.has_priority()) {
    return base::nullopt;
  }

  base::Value sequencing_dictionary{base::Value::Type::DICTIONARY};
  sequencing_dictionary.SetIntKey(GetSequencingIdPath(),
                                  sequencing_information.sequencing_id());
  sequencing_dictionary.SetIntKey(GetGenerationIdPath(),
                                  sequencing_information.generation_id());
  sequencing_dictionary.SetIntKey(GetPriorityPath(),
                                  sequencing_information.priority());
  return sequencing_dictionary;
}

// static
std::string EncryptedReportingJobConfiguration::
    SequencingInformationDictionaryBuilder::GetSequencingIdPath() {
  return kSequencingId_;
}

// static
std::string EncryptedReportingJobConfiguration::
    SequencingInformationDictionaryBuilder::GetGenerationIdPath() {
  return kGenerationId_;
}

// static
std::string EncryptedReportingJobConfiguration::
    SequencingInformationDictionaryBuilder::GetPriorityPath() {
  return kPriority_;
}

// static
base::Optional<base::Value> EncryptedReportingJobConfiguration::
    EncryptionInfoDictionaryBuilder::ConvertEncryptionInfoProtoToValue(
        const ::reporting::EncryptionInfo& encryption_info) {
  // EncryptionInfo requires both fields are set.
  if (!encryption_info.has_encryption_key() ||
      !encryption_info.has_public_key_id()) {
    return base::nullopt;
  }

  base::Value encryption_info_dictionary{base::Value::Type::DICTIONARY};
  encryption_info_dictionary.SetStringKey(GetEncryptionKeyPath(),
                                          encryption_info.encryption_key());
  encryption_info_dictionary.SetIntKey(GetPublicKeyIdPath(),
                                       encryption_info.public_key_id());
  return encryption_info_dictionary;
}

// static
std::string EncryptedReportingJobConfiguration::
    EncryptionInfoDictionaryBuilder::GetEncryptionKeyPath() {
  return kEncryptionKey_;
}

// static
std::string EncryptedReportingJobConfiguration::
    EncryptionInfoDictionaryBuilder::GetPublicKeyIdPath() {
  return kPublicKeyId_;
}

}  // namespace policy
