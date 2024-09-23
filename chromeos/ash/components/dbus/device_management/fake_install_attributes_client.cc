// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
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
  if (locked_) {
    LoadInstallAttributes();
  }
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
    const ::device_management::InstallAttributesGetRequest& request,
    InstallAttributesGetCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesFinalize(
    const ::device_management::InstallAttributesFinalizeRequest& request,
    InstallAttributesFinalizeCallback callback) {
  NOTIMPLEMENTED();
}
void FakeInstallAttributesClient::InstallAttributesGetStatus(
    const ::device_management::InstallAttributesGetStatusRequest& request,
    InstallAttributesGetStatusCallback callback) {
  std::optional<::device_management::InstallAttributesGetStatusReply> reply =
      BlockingInstallAttributesGetStatus(request);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}
void FakeInstallAttributesClient::RemoveFirmwareManagementParameters(
    const ::device_management::RemoveFirmwareManagementParametersRequest& request,
    RemoveFirmwareManagementParametersCallback callback) {
  remove_firmware_management_parameters_from_tpm_call_count_++;
  fwmp_flags_ = std::nullopt;
  ReturnProtobufMethodCallback(
      ::device_management::RemoveFirmwareManagementParametersReply(),
      std::move(callback));
}
void FakeInstallAttributesClient::SetFirmwareManagementParameters(
    const ::device_management::SetFirmwareManagementParametersRequest& request,
    SetFirmwareManagementParametersCallback callback) {
  if (request.has_fwmp()) {
    fwmp_flags_ = request.fwmp().flags();
  }
  ReturnProtobufMethodCallback(
      ::device_management::SetFirmwareManagementParametersReply(),
      std::move(callback));
}
void FakeInstallAttributesClient::GetFirmwareManagementParameters(
    const ::device_management::GetFirmwareManagementParametersRequest& request,
    GetFirmwareManagementParametersCallback callback) {
  auto reply = ::device_management::GetFirmwareManagementParametersReply();
  if (fwmp_flags_) {
    reply.mutable_fwmp()->set_flags(*fwmp_flags_);
  } else {
    reply.set_error(
        ::device_management::
            DEVICE_MANAGEMENT_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_INVALID);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
std::optional<::device_management::InstallAttributesGetReply>
FakeInstallAttributesClient::BlockingInstallAttributesGet(
    const ::device_management::InstallAttributesGetRequest& request) {
  ::device_management::InstallAttributesGetReply reply;
  if (install_attrs_.find(request.name()) != install_attrs_.end()) {
    reply.set_value(install_attrs_[request.name()]);
  } else {
    reply.set_error(::device_management::DeviceManagementErrorCode::
                        DEVICE_MANAGEMENT_ERROR_INSTALL_ATTRIBUTES_GET_FAILED);
  }
  return reply;
}
std::optional<::device_management::InstallAttributesSetReply>
FakeInstallAttributesClient::BlockingInstallAttributesSet(
    const ::device_management::InstallAttributesSetRequest& request) {
  ::device_management::InstallAttributesSetReply reply;
  install_attrs_[request.name()] = request.value();
  return reply;
}
std::optional<::device_management::InstallAttributesFinalizeReply>
FakeInstallAttributesClient::BlockingInstallAttributesFinalize(
    const ::device_management::InstallAttributesFinalizeRequest& request) {
  locked_ = true;
  ::device_management::InstallAttributesFinalizeReply reply;

  // Persist the install attributes so that they can be reloaded if the
  // browser is restarted. This is used for ease of development when device
  // enrollment is required.
  base::FilePath cache_path;
  if (!base::PathService::Get(chromeos::dbus_paths::FILE_INSTALL_ATTRIBUTES,
                              &cache_path)) {
    reply.set_error(::device_management::DeviceManagementErrorCode::
                        DEVICE_MANAGEMENT_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED);
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
  base::WriteFile(cache_path, result);

  return reply;
}
std::optional<::device_management::InstallAttributesGetStatusReply>
FakeInstallAttributesClient::BlockingInstallAttributesGetStatus(
    const ::device_management::InstallAttributesGetStatusRequest& request) {
  ::device_management::InstallAttributesGetStatusReply reply;
  if (locked_) {
    reply.set_state(::device_management::InstallAttributesState::VALID);
  } else {
    reply.set_state(::device_management::InstallAttributesState::FIRST_INSTALL);
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
  if (!is_available) {
    return;
  }

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(true);
  }
}

void FakeInstallAttributesClient::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(false);
  }
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
