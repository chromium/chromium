// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/manifest_util.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chrome/updater/win/protocol_parser_xml.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

absl::optional<base::FilePath> GetOfflineManifest(
    const base::FilePath& offline_dir,
    const std::string& app_id) {
  // Check manifest with fixed name first.
  base::FilePath manifest_path = offline_dir.AppendASCII("OfflineManifest.gup");
  if (base::PathExists(manifest_path))
    return manifest_path;

  // Then check the legacy app specific manifest.
  manifest_path =
      offline_dir.AppendASCII(app_id).AddExtension(FILE_PATH_LITERAL(".gup"));
  return base::PathExists(manifest_path)
             ? absl::optional<base::FilePath>(manifest_path)
             : absl::nullopt;
}

std::unique_ptr<ProtocolParserXML> ParseOfflineManifest(
    const base::FilePath& offline_dir,
    const std::string& app_id) {
  absl::optional<base::FilePath> manifest_path =
      GetOfflineManifest(offline_dir, app_id);
  if (!manifest_path) {
    VLOG(2) << "Cannot find manifest file in: " << offline_dir;
    return nullptr;
  }

  int64_t file_size = 0;
  if (!base::GetFileSize(manifest_path.value(), &file_size)) {
    VLOG(2) << "Cannot determine manifest file size.";
    return nullptr;
  }

  constexpr int64_t kMaxManifestSize = 1024 * 1024;
  if (file_size > kMaxManifestSize) {
    VLOG(2) << "Manifest file is too large.";
    return nullptr;
  }

  std::string contents(file_size, '\0');
  if (base::ReadFile(manifest_path.value(), &contents[0], file_size) == -1) {
    VLOG(2) << "Failed to load manifest file: " << manifest_path.value();
    return nullptr;
  }
  auto xml_parser = std::make_unique<ProtocolParserXML>();
  if (!xml_parser->Parse(contents)) {
    VLOG(2) << "Failed to parse XML manifest file: " << manifest_path.value();
    return nullptr;
  }

  return xml_parser;
}

}  // namespace

void ReadInstallCommandFromManifest(const base::FilePath& offline_dir,
                                    const std::string& app_id,
                                    const std::string& install_data_index,
                                    base::FilePath& installer_path,
                                    std::string& install_args,
                                    std::string& install_data) {
  std::unique_ptr<ProtocolParserXML> manifest_parser =
      ParseOfflineManifest(offline_dir, app_id);
  if (!manifest_parser) {
    return;
  }

  const std::vector<update_client::ProtocolParser::Result>& results =
      manifest_parser->results().list;
  auto it = base::ranges::find_if(
      results, [&app_id](const update_client::ProtocolParser::Result& result) {
        return base::EqualsCaseInsensitiveASCII(result.extension_id, app_id);
      });
  if (it == std::end(results)) {
    VLOG(2) << "No manifest data for app: " << app_id;
    return;
  }
  installer_path = offline_dir.AppendASCII(it->manifest.run);
  install_args = it->manifest.arguments;

  if (!install_data_index.empty()) {
    auto data_iter = base::ranges::find_if(
        it->data, [&install_data_index](
                      const update_client::ProtocolParser::Result::Data& data) {
          return install_data_index == data.install_data_index;
        });
    if (data_iter == std::end(it->data)) {
      VLOG(2) << "Install data index not found: " << install_data_index;
      return;
    }
    install_data = data_iter->text;
  }
}

}  // namespace updater
