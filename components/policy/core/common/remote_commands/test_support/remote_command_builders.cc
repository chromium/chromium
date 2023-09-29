// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"

#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"

namespace em = enterprise_management;

namespace policy {

enterprise_management::RemoteCommand RemoteCommandBuilder::Build() {
  return std::move(result_);
}

RemoteCommandBuilder& RemoteCommandBuilder::SetCommandId(int64_t value) {
  result_.set_command_id(value);
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::ClearCommandId() {
  result_.clear_command_id();
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::SetType(
    em::RemoteCommand::Type type) {
  result_.set_type(type);
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::ClearType() {
  result_.clear_type();
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::SetPayload(
    const std::string& payload) {
  result_.set_payload(payload);
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::SetTargetDeviceId(
    const std::string& value) {
  result_.set_target_device_id(value);
  return *this;
}

em::SignedData SignedDataBuilder::Build() {
  return BuildSignedData(command_.Build());
}

SignedDataBuilder& SignedDataBuilder::SetCommandId(int id) {
  command_.SetCommandId(id);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::ClearCommandId() {
  command_.ClearCommandId();
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetCommandType(
    em::RemoteCommand::Type type) {
  command_.SetType(type);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::ClearCommandType() {
  command_.ClearType();
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetCommandPayload(
    const std::string& value) {
  command_.SetPayload(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetTargetDeviceId(
    const std::string& value) {
  command_.SetTargetDeviceId(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetSignedData(const std::string& value) {
  signed_data.set_data(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetSignature(const std::string& value) {
  signed_data.set_signature(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetPolicyType(const std::string& value) {
  policy_data.set_policy_type(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::SetPolicyValue(const std::string& value) {
  policy_data.set_policy_value(value);
  return *this;
}

em::SignedData SignedDataBuilder::BuildSignedData(
    const em::RemoteCommand& command) {
  if (!policy_data.has_policy_type()) {
    policy_data.set_policy_type("google/chromeos/remotecommand");
  }

  if (!policy_data.has_policy_value()) {
    command.SerializeToString(policy_data.mutable_policy_value());
  }

  if (!signed_data.has_data()) {
    policy_data.SerializeToString(signed_data.mutable_data());
  }

  if (!signed_data.has_signature()) {
    signed_data.set_signature(SignDataWithTestKey(
        signed_data.data(), RemoteCommandsService::GetSignatureType()));
  }

  return signed_data;
}

}  // namespace policy
