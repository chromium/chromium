// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/file_provider_impl.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/services/assistant/utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {
namespace assistant {
namespace {

constexpr int kReadFileSizeLimitInBytes = 10 * 1024 * 1024;

}  // namespace

FileProviderImpl::FileProviderImpl()
    : root_path_(
          GetRootPath().Append(FILE_PATH_LITERAL("google-assistant-library"))) {
}

FileProviderImpl::~FileProviderImpl() = default;

std::string FileProviderImpl::ReadFile(const std::string& path) {
  std::string data;
  base::ReadFileToStringWithMaxSize(root_path_.Append(path), &data,
                                    kReadFileSizeLimitInBytes);
  return data;
}

bool FileProviderImpl::WriteFile(const std::string& path,
                                 const std::string& data) {
  base::FilePath full_path = root_path_.Append(path);
  if (!base::PathExists(full_path.DirName()) &&
      !base::CreateDirectory(full_path.DirName()))
    return false;

  // Create a temp file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(full_path.DirName(), &temp_file)) {
    return false;
  }

  // Write to the tmp file.
  const int size = data.size();
  int written_size = base::WriteFile(temp_file, data.data(), size);
  if (written_size != size) {
    return false;
  }

  // Replace the current file with the temp file.
  if (!base::ReplaceFile(temp_file, full_path, nullptr)) {
    return false;
  }

  return true;
}

std::string FileProviderImpl::ReadSecureFile(const std::string& path) {
  return ReadFile(path);
}

bool FileProviderImpl::WriteSecureFile(const std::string& path,
                                       const std::string& data) {
  // No need to encrypt since |root_path_| should be inside the primary user's
  // cryptohome and is already encrypted.
  return WriteFile(path, data);
}

void FileProviderImpl::CleanAssistantData() {
  base::DeleteFile(root_path_, true);
}

bool FileProviderImpl::GetResource(uint16_t resource_id, std::string* out) {
  int chrome_resource_id = -1;
  switch (resource_id) {
    case assistant_client::resource_ids::kGeneralError:
      chrome_resource_id = IDR_ASSISTANT_SPEECH_RECOGNITION_ERROR;
      break;
    case assistant_client::resource_ids::kWifiNeedsSetupError:
    case assistant_client::resource_ids::kWifiNotConnectedError:
    case assistant_client::resource_ids::kWifiCannotConnectError:
    case assistant_client::resource_ids::kNetworkConnectingError:
    // These above do not apply to ChromeOS, but let it fall through to get a
    // generic error.
    case assistant_client::resource_ids::kNetworkCannotReachServerError:
      chrome_resource_id = IDR_ASSISTANT_NO_INTERNET_ERROR;
      break;
    default:
      break;
  }

  if (chrome_resource_id < 0)
    return false;

  auto data = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      chrome_resource_id);
  out->assign(data.data(), data.length());
  return true;
}

}  // namespace assistant
}  // namespace chromeos
