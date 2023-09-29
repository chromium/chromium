// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_REMOTE_COMMAND_BUILDERS_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_REMOTE_COMMAND_BUILDERS_H_

#include <string>

#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// Builder class to construct `enterprise_management::RemoteCommand`.
//
// It also auto-assigns a unique command id to the `RemoteCommand` that is
// one higher than the last command id it previously assigned. You can of course
// assign your own command id if more control is needed.
class RemoteCommandBuilder {
 public:
  RemoteCommandBuilder() = default;

  RemoteCommandBuilder(const RemoteCommandBuilder&) = default;
  RemoteCommandBuilder& operator=(const RemoteCommandBuilder&) = default;
  RemoteCommandBuilder(RemoteCommandBuilder&&) = default;
  RemoteCommandBuilder& operator=(RemoteCommandBuilder&&) = default;
  ~RemoteCommandBuilder() = default;

  enterprise_management::RemoteCommand Build();

  RemoteCommandBuilder& SetCommandId(int64_t value);
  RemoteCommandBuilder& ClearCommandId();
  RemoteCommandBuilder& SetType(enterprise_management::RemoteCommand::Type);
  RemoteCommandBuilder& ClearType();
  RemoteCommandBuilder& SetPayload(const std::string& payload);
  RemoteCommandBuilder& SetTargetDeviceId(const std::string& value);

 private:
  enterprise_management::RemoteCommand result_;
};

// Builder class to construct `enterprise_management::SignedData`.
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

  SignedDataBuilder& SetCommandId(int id);
  SignedDataBuilder& ClearCommandId();
  SignedDataBuilder& SetCommandType(
      enterprise_management::RemoteCommand::Type type);
  SignedDataBuilder& ClearCommandType();
  SignedDataBuilder& SetCommandPayload(const std::string& value);
  SignedDataBuilder& SetTargetDeviceId(const std::string& value);
  SignedDataBuilder& SetSignedData(const std::string& value);
  SignedDataBuilder& SetSignature(const std::string& value);
  SignedDataBuilder& SetPolicyType(const std::string& value);
  SignedDataBuilder& SetPolicyValue(const std::string& value);

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
