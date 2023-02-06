// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_REMOTE_COMMAND_BUILDERS_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_REMOTE_COMMAND_BUILDERS_H_

#include <string>

#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// Builder class to construct |enterprise_management::RemoteCommand|.
// Used in unit and browser tests.
class RemoteCommandBuilder {
 public:
  RemoteCommandBuilder() = default;
  RemoteCommandBuilder(const RemoteCommandBuilder&) = delete;
  RemoteCommandBuilder& operator=(const RemoteCommandBuilder&) = delete;
  RemoteCommandBuilder(RemoteCommandBuilder&&) = default;
  RemoteCommandBuilder& operator=(RemoteCommandBuilder&&) = default;
  ~RemoteCommandBuilder() = default;

  enterprise_management::RemoteCommand Build() { return result_; }

  RemoteCommandBuilder& WithId(int id);
  RemoteCommandBuilder& WithoutId();
  RemoteCommandBuilder& WithType(
      enterprise_management::RemoteCommand::Type type);
  RemoteCommandBuilder& WithoutType();
  RemoteCommandBuilder& WithPayload(const std::string& payload);
  RemoteCommandBuilder& WithTargetDeviceId(const std::string& value);

 private:
  enterprise_management::RemoteCommand result_;
};

// Builder class to construct |enterprise_management::SignedData|.
// Used in unit and browser tests.
class SignedDataBuilder {
 public:
  SignedDataBuilder() = default;
  SignedDataBuilder(const SignedDataBuilder&) = delete;
  SignedDataBuilder& operator=(const SignedDataBuilder&) = delete;
  SignedDataBuilder(SignedDataBuilder&&) = default;
  SignedDataBuilder& operator=(SignedDataBuilder&&) = default;
  ~SignedDataBuilder() = default;

  enterprise_management::SignedData Build();

  SignedDataBuilder& WithCommandId(int id);
  SignedDataBuilder& WithoutCommandId();
  SignedDataBuilder& WithCommandType(
      enterprise_management::RemoteCommand::Type type);
  SignedDataBuilder& WithoutCommandType();
  SignedDataBuilder& WithCommandPayload(const std::string& value);
  SignedDataBuilder& WithTargetDeviceId(const std::string& value);
  SignedDataBuilder& WithSignedData(const std::string& value);
  SignedDataBuilder& WithSignature(const std::string& value);
  SignedDataBuilder& WithPolicyType(const std::string& value);
  SignedDataBuilder& WithPolicyValue(const std::string& value);

 private:
  // The signed data defaults to correctly signing the remote command,
  // unless it was explicitly overwritten during the test.
  enterprise_management::SignedData BuildSignedData(
      const enterprise_management::RemoteCommand& command);

  RemoteCommandBuilder command_;
  enterprise_management::PolicyData policy_data;
  enterprise_management::SignedData signed_data;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_REMOTE_COMMAND_BUILDERS_H_
