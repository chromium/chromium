// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/fake_install_attributes_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/policy/proto/install_attributes.pb.h"

namespace ash {

namespace {

// Buffer size for reading install attributes file. 16k should be plenty. The
// file contains six attributes only (see InstallAttributes::LockDevice).
constexpr size_t kInstallAttributesFileMaxSize = 16384;

// Used to track the fake instance, mirrors the instance in the base class.
FakeInstallAttributesClient* g_instance = nullptr;

}  // namespace

FakeInstallAttributesClient::FakeInstallAttributesClient() {
  DCHECK(!g_instance);
  g_instance = this;

  base::FilePath cache_path;
  locked_ = base::PathService::Get(
                chromeos::dbus_paths::FILE_INSTALL_ATTRIBUTES, &cache_path) &&
            base::PathExists(cache_path);
  if (locked_)
    LoadInstallAttributes();
}

FakeInstallAttributesClient::~FakeInstallAttributesClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeInstallAttributesClient* FakeInstallAttributesClient::Get() {
  return g_instance;
}

void FakeInstallAttributesClient::InstallAttributesGet(
    const ::user_data_auth::InstallAttributesGetRequest& request,
    InstallAttributesGetCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesFinalize(
    const ::user_data_auth::InstallAttributesFinalizeRequest& request,
    InstallAttributesFinalizeCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesGetStatus(
    const ::user_data_auth::InstallAttributesGetStatusRequest& request,
    InstallAttributesGetStatusCallback callback) {
  absl::optional<::user_data_auth::InstallAttributesGetStatusReply> reply =
      BlockingInstallAttributesGetStatus(request);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}
void FakeInstallAttributesClient::RemoveFirmwareManagementParameters(
    const ::user_data_auth::RemoveFirmwareManagementParametersRequest& request,
    RemoveFirmwareManagementParametersCallback callback) {
  remove_firmware_management_parameters_from_tpm_call_count_++;
  ReturnProtobufMethodCallback(
      ::user_data_auth::RemoveFirmwareManagementParametersReply(),
      std::move(callback));
}
void FakeInstallAttributesClient::SetFirmwareManagementParameters(
    const ::user_data_auth::SetFirmwareManagementParametersRequest& request,
    SetFirmwareManagementParametersCallback callback) {
  ReturnProtobufMethodCallback(
      ::user_data_auth::SetFirmwareManagementParametersReply(),
      std::move(callback));
}
absl::optional<::user_data_auth::InstallAttributesGetReply>
FakeInstallAttributesClient::BlockingInstallAttributesGet(
    const ::user_data_auth::InstallAttributesGetRequest& request) {
  ::user_data_auth::InstallAttributesGetReply reply;
  if (install_attrs_.find(request.name()) != install_attrs_.end()) {
    reply.set_value(install_attrs_[request.name()]);
  } else {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_GET_FAILED);
  }
  return reply;
}
absl::optional<::user_data_auth::InstallAttributesSetReply>
FakeInstallAttributesClient::BlockingInstallAttributesSet(
    const ::user_data_auth::InstallAttributesSetRequest& request) {
  ::user_data_auth::InstallAttributesSetReply reply;
  install_attrs_[request.name()] = request.value();
  return reply;
}
absl::optional<::user_data_auth::InstallAttributesFinalizeReply>
FakeInstallAttributesClient::BlockingInstallAttributesFinalize(
    const ::user_data_auth::InstallAttributesFinalizeRequest& request) {
  locked_ = true;
  ::user_data_auth::InstallAttributesFinalizeReply reply;

  // Persist the install attributes so that they can be reloaded if the
  // browser is restarted. This is used for ease of development when device
  // enrollment is required.
  base::FilePath cache_path;
  if (!base::PathService::Get(chromeos::dbus_paths::FILE_INSTALL_ATTRIBUTES,
                              &cache_path)) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED);
    return reply;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  for (const auto& it : install_attrs_) {
    const std::string& name = it.first;
    const std::string& value = it.second;
    cryptohome::SerializedInstallAttributes::Attribute* attr_entry =
        install_attrs_proto.add_attributes();
    attr_entry->set_name(name);
    attr_entry->set_value(value);
  }

  std::string result;
  install_attrs_proto.SerializeToString(&result);

  // The real implementation does a blocking wait on the dbus call; the fake
  // implementation must have this file written before returning.
  base::ScopedAllowBlockingForTesting allow_io;
  base::WriteFile(cache_path, result.data(), result.size());

  return reply;
}
absl::optional<::user_data_auth::InstallAttributesGetStatusReply>
FakeInstallAttributesClient::BlockingInstallAttributesGetStatus(
    const ::user_data_auth::InstallAttributesGetStatusRequest& request) {
  ::user_data_auth::InstallAttributesGetStatusReply reply;
  if (locked_) {
    reply.set_state(user_data_auth::InstallAttributesState::VALID);
  } else {
    reply.set_state(user_data_auth::InstallAttributesState::FIRST_INSTALL);
  }
  return reply;
}

void FakeInstallAttributesClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeInstallAttributesClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(true);
}

void FakeInstallAttributesClient::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(false);
}

template <typename ReplyType>
void FakeInstallAttributesClient::ReturnProtobufMethodCallback(
    const ReplyType& reply,
    chromeos::DBusMethodCallback<ReplyType> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

bool FakeInstallAttributesClient::LoadInstallAttributes() {
  base::FilePath cache_file;
  const bool file_exists =
      base::PathService::Get(chromeos::dbus_paths::FILE_INSTALL_ATTRIBUTES,
                             &cache_file) &&
      base::PathExists(cache_file);
  DCHECK(file_exists);
  // Mostly copied from
  // chromeos/ash/components/install_attributes/install_attributes.cc.
  std::string file_blob;
  if (!base::ReadFileToStringWithMaxSize(cache_file, &file_blob,
                                         kInstallAttributesFileMaxSize)) {
    PLOG(ERROR) << "Failed to read " << cache_file.value();
    return false;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  if (!install_attrs_proto.ParseFromString(file_blob)) {
    LOG(ERROR) << "Failed to parse install attributes cache.";
    return false;
  }

  for (const auto& entry : install_attrs_proto.attributes()) {
    install_attrs_[entry.name()].assign(
        entry.value().data(), entry.value().data() + entry.value().size());
  }

  return true;
}

}  // namespace ash
