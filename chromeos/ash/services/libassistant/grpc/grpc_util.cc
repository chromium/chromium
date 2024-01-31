// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"

#include "base/check_op.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "third_party/grpc/src/include/grpcpp/impl/codegen/proto_utils.h"
#include "third_party/grpc/src/include/grpcpp/support/proto_buffer_reader.h"
#include "third_party/grpc/src/include/grpcpp/support/proto_buffer_writer.h"

namespace ash::libassistant {

namespace {

constexpr char kForwardSlash[] = "/";
constexpr char kDomainPrefix[] = "unix://";

std::string GetBaseDirectory(bool is_chromeos_device) {
  return is_chromeos_device ? chromeos::assistant::kSocketDirectory
                            : chromeos::assistant::kSocketTempDirectory;
}

}  // namespace

grpc_local_connect_type GetGrpcLocalConnectType(
    const std::string& server_address) {
  // We only support unix socket on our platform.
  DCHECK_EQ(server_address.compare(0, 4, "unix"), 0);
  return UDS;
}

grpc::Status GrpcSerializeProto(const google::protobuf::MessageLite& src,
                                grpc::ByteBuffer* dst) {
  bool own_buffer;
  return grpc::GenericSerialize<grpc::ProtoBufferWriter,
                                google::protobuf::MessageLite>(src, dst,
                                                               &own_buffer);
}

bool GrpcParseProto(grpc::ByteBuffer* src, google::protobuf::MessageLite* dst) {
  grpc::ProtoBufferReader reader(src);
  return dst->ParseFromZeroCopyStream(&reader);
}

std::string GetLibassistGrpcMethodName(const std::string& service,
                                       const std::string& method) {
  return std::string(kForwardSlash) +
         chromeos::assistant::kLibassistGrpcServicePrefix + service +
         kForwardSlash + method;
}

std::string GetLibassistGrpcServiceName(const std::string& event) {
  return chromeos::assistant::kLibassistGrpcServicePrefix + event +
         chromeos::assistant::kHandlerInterface;
}

std::string GetAssistantSocketFileName(bool is_chromeos_device) {
  return GetBaseDirectory(is_chromeos_device) +
         chromeos::assistant::kAssistantSocketName;
}

std::string GetLibassistantSocketFileName(bool is_chromeos_device) {
  return GetBaseDirectory(is_chromeos_device) +
         chromeos::assistant::kLibassistantSocketName;
}

std::string GetHttpConnectionSocketFileName(bool is_chromeos_device) {
  return GetBaseDirectory(is_chromeos_device) +
         chromeos::assistant::kHttpConnectionSocketName;
}

std::string GetAssistantServiceAddress(bool is_chromeos_device) {
  return std::string(kDomainPrefix) +
         GetAssistantSocketFileName(is_chromeos_device);
}

std::string GetLibassistantServiceAddress(bool is_chromeos_device) {
  return std::string(kDomainPrefix) +
         GetLibassistantSocketFileName(is_chromeos_device);
}

std::string GetHttpConnectionServiceAddress(bool is_chromeos_device) {
  return std::string(kDomainPrefix) +
         GetHttpConnectionSocketFileName(is_chromeos_device);
}

}  // namespace ash::libassistant
