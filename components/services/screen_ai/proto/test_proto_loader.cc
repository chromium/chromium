// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/proto/test_proto_loader.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "google/protobuf/text_format.h"

namespace test_proto_loader {

const google::protobuf::Message* TestProtoLoader::GetPrototype(
    base::FilePath descriptor_path,
    std::string package,
    std::string name) {
  std::string file_contents;

  if (!base::ReadFileToString(descriptor_path, &file_contents)) {
    LOG(ERROR) << "Couldn't load contents of " << descriptor_path;
    return nullptr;
  }

  if (!descriptor_set_.ParseFromString(file_contents)) {
    LOG(ERROR) << "Couldn't parse descriptor from " << descriptor_path;
    return nullptr;
  }

  for (int file_i = 0; file_i < descriptor_set_.file_size(); ++file_i) {
    const google::protobuf::FileDescriptorProto& file =
        descriptor_set_.file(file_i);
    if (file.package() != package)
      continue;
    const google::protobuf::FileDescriptor* descriptor =
        descriptor_pool_.BuildFile(file);
    for (int message_type_i = 0;
         message_type_i < descriptor->message_type_count(); ++message_type_i) {
      const google::protobuf::Descriptor* message_type =
          descriptor->message_type(message_type_i);
      if (message_type->name() != name)
        continue;
      return dynamic_message_factory_.GetPrototype(message_type);
    }
  }
  LOG(ERROR) << "Couldn't find " << package << "." << name << "in "
             << descriptor_path;
  return nullptr;
}

bool TestProtoLoader::ParseFromText(
    const base::FilePath& descriptor_path,
    const std::string& proto_text,
    google::protobuf::MessageLite& destination) {
  // Load the descriptors and find the one for |destination|.
  std::string package, name;
  std::vector<std::string> type_name_parts =
      base::SplitString(destination.GetTypeName(), ".", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  if (type_name_parts.size() != 2)
    return false;

  const google::protobuf::Message* prototype =
      GetPrototype(descriptor_path, /*package =*/type_name_parts[0],
                   /* name = */ type_name_parts[1]);
  if (!prototype)
    return false;

  // Parse the text using the descriptor-generated message and send it to
  // |destination|.
  std::unique_ptr<google::protobuf::Message> message(prototype->New());
  google::protobuf::TextFormat::ParseFromString(proto_text, message.get());
  destination.ParseFromString(message->SerializeAsString());

  return true;
}

// static
bool TestProtoLoader::LoadTextProto(
    const base::FilePath& proto_file_path,
    const char* proto_descriptor_relative_file_path,
    google::protobuf::MessageLite& proto) {
  std::string file_content;
  if (!base::ReadFileToString(proto_file_path, &file_content)) {
    LOG(ERROR) << "Failed to read expected proto from: " << proto_file_path;
    return false;
  }

  base::FilePath descriptor_full_path;
  if (!base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT,
                              &descriptor_full_path)) {
    LOG(ERROR) << "Generated test data root not found!";
    return false;
  }
  descriptor_full_path =
      descriptor_full_path.AppendASCII(proto_descriptor_relative_file_path);

  test_proto_loader::TestProtoLoader loader;
  return loader.ParseFromText(descriptor_full_path, file_content, proto);
}

}  // namespace test_proto_loader
