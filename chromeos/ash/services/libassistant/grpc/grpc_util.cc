// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"

#include "base/check_op.h"
#include "third_party/grpc/src/include/grpcpp/impl/codegen/proto_utils.h"
#include "third_party/grpc/src/include/grpcpp/support/proto_buffer_reader.h"
#include "third_party/grpc/src/include/grpcpp/support/proto_buffer_writer.h"

namespace ash::libassistant {

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

}  // namespace ash::libassistant
