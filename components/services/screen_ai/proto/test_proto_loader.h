// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PROTO_TEST_PROTO_LOADER_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PROTO_TEST_PROTO_LOADER_H_

#include "base/files/file_path.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"

namespace test_proto_loader {

// This class works around the fact that chrome only includes the lite runtime
// of protobufs. Lite protobufs inherit from |MessageLite| and cannot be used to
// parse from text format. Parsing from text
// format is useful in tests. We cannot include the full version of a protobuf
// in test code because it would clash with the lite version.
//
// This class uses the protobuf descriptors (generated at compile time) to
// to generate a |Message| that can be used to parse from text. This message
// can then serialize to binary which can be parsed by the |MessageLite|.
//
// If needed, we can move this class to a folder that would be available to
// other tests.
class TestProtoLoader {
 public:
  TestProtoLoader() = default;
  ~TestProtoLoader() = default;
  TestProtoLoader(const TestProtoLoader&) = delete;
  TestProtoLoader& operator=(const TestProtoLoader&) = delete;

  bool ParseFromText(const base::FilePath& descriptor_path,
                     const std::string& proto_text,
                     google::protobuf::MessageLite& destination);

  // Loads a text proto file from |proto_file_path| into |proto|, where the
  // descriptor of the proto exists in |proto_descriptor_relative_file_path|,
  // relative to DIR_GEN_TEST_DATA_ROOT.
  static bool LoadTextProto(const base::FilePath& proto_file_path,
                            const char* proto_descriptor_relative_file_path,
                            google::protobuf::MessageLite& proto);

 private:
  const google::protobuf::Message* GetPrototype(base::FilePath descriptor_path,
                                                std::string package,
                                                std::string name);

  google::protobuf::DescriptorPool descriptor_pool_;
  google::protobuf::FileDescriptorSet descriptor_set_;
  google::protobuf::DynamicMessageFactory dynamic_message_factory_;
};

}  // namespace test_proto_loader

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PROTO_TEST_PROTO_LOADER_H_
