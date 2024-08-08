// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <stddef.h>
#include <stdint.h>

#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

bool BinaryFeatureExtractor::ExtractImageFeaturesFromData(
    base::span<const uint8_t> data,
    ExtractHeadersOption options,
    ClientDownloadRequest_ImageHeaders* image_headers,
    google::protobuf::RepeatedPtrField<std::string>* signed_data) {
  MachOImageReader image_reader;
  // TODO(crbug.com/356368033): MachOImageReader should also take a span.
  if (!image_reader.Initialize(data.data(), data.size())) {
    return false;
  }

  // If the image is fat, get all its MachO images. Otherwise, just scan
  // the thin image.
  std::vector<MachOImageReader*> images;
  if (image_reader.IsFat())
    images = image_reader.GetFatImages();
  else
    images.push_back(&image_reader);

  for (auto* mach_o_reader : images) {
    // Record the entire mach_header struct.
    auto* mach_o_headers = image_headers->mutable_mach_o_headers()->Add();
    if (mach_o_reader->Is64Bit()) {
      const mach_header_64* header = mach_o_reader->GetMachHeader64();
      mach_o_headers->set_mach_header(header, sizeof(*header));
    } else {
      const mach_header* header = mach_o_reader->GetMachHeader();
      mach_o_headers->set_mach_header(header, sizeof(*header));
    }

    // Store the load commands for the Mach-O binary.
    auto* proto_load_commands = mach_o_headers->mutable_load_commands();
    const std::vector<MachOImageReader::LoadCommand>& load_commands =
        mach_o_reader->GetLoadCommands();
    for (const auto& load_command : load_commands) {
      auto* proto_load_command = proto_load_commands->Add();
      proto_load_command->set_command_id(load_command.cmd());
      proto_load_command->set_command(&load_command.data[0],
                                      load_command.data.size());
    }

    // Get the signature information.
    if (signed_data) {
      std::vector<uint8_t> code_signature;
      if (mach_o_reader->GetCodeSignatureInfo(&code_signature)) {
        signed_data->Add()->append(
            reinterpret_cast<const char*>(&code_signature[0]),
            code_signature.size());
      }
    }
  }

  return true;
}

}  // namespace safe_browsing
