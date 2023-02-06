// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"

#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"

namespace em = enterprise_management;

namespace policy {

RemoteCommandBuilder& RemoteCommandBuilder::WithId(int id) {
  result_.set_command_id(id);
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::WithoutId() {
  result_.clear_command_id();
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::WithType(
    em::RemoteCommand::Type type) {
  result_.set_type(type);
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::WithoutType() {
  result_.clear_type();
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::WithPayload(
    const std::string& payload) {
  result_.set_payload(payload);
  return *this;
}

RemoteCommandBuilder& RemoteCommandBuilder::WithTargetDeviceId(
    const std::string& value) {
  result_.set_target_device_id(value);
  return *this;
}

em::SignedData SignedDataBuilder::Build() {
  return BuildSignedData(command_.Build());
}

SignedDataBuilder& SignedDataBuilder::WithCommandId(int id) {
  command_.WithId(id);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithoutCommandId() {
  command_.WithoutId();
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithCommandType(
    em::RemoteCommand::Type type) {
  command_.WithType(type);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithoutCommandType() {
  command_.WithoutType();
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithCommandPayload(
    const std::string& value) {
  command_.WithPayload(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithTargetDeviceId(
    const std::string& value) {
  command_.WithTargetDeviceId(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithSignedData(const std::string& value) {
  signed_data.set_data(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithSignature(const std::string& value) {
  signed_data.set_signature(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithPolicyType(const std::string& value) {
  policy_data.set_policy_type(value);
  return *this;
}

SignedDataBuilder& SignedDataBuilder::WithPolicyValue(
    const std::string& value) {
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
