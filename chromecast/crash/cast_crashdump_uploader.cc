// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crashdump_uploader.h"

#include <sys/stat.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
// TODO(slan): Find a replacement for LibcurlWrapper in Chromium to remove the
// breakpad dependency.
#include "third_party/breakpad/breakpad/src/common/linux/libcurl_wrapper.h"

namespace chromecast {
namespace {

// Keep these in sync with
// //third_party/breakpad/breakpad/src/client/mac/sender/uploader.mm
const char kProdKey[] = "prod";
const char kVerKey[] = "ver";
const char kGuidKey[] = "guid";
const char kPtimeKey[] = "ptime";
const char kCtimeKey[] = "ctime";
const char kEmailKey[] = "email";
const char kCommentsKey[] = "comments";

}  // namespace

CastCrashdumpData::CastCrashdumpData() {
}

CastCrashdumpData::CastCrashdumpData(const CastCrashdumpData& other) = default;

CastCrashdumpData::~CastCrashdumpData() {
}

CastCrashdumpUploader::CastCrashdumpUploader(const CastCrashdumpData& data)
    : CastCrashdumpUploader(
          data,
          std::make_unique<google_breakpad::LibcurlWrapper>()) {}

CastCrashdumpUploader::CastCrashdumpUploader(
    const CastCrashdumpData& data,
    std::unique_ptr<google_breakpad::LibcurlWrapper> http_layer)
    : http_layer_(std::move(http_layer)), data_(data) {
  DCHECK(http_layer_);
}

CastCrashdumpUploader::~CastCrashdumpUploader() {
}

bool CastCrashdumpUploader::AddAttachment(const std::string& label,
                                          const std::string& filename) {
  int64_t file_size = 0;
  if (!base::GetFileSize(base::FilePath(filename), &file_size)) {
    LOG(WARNING) << "file size of " << filename << " not readable";
    return false;
  }
  LOG(INFO) << "file size of " << filename << ": " << file_size;
  attachments_[label] = filename;
  return true;
}

bool CastCrashdumpUploader::CheckRequiredParametersArePresent() {
  return !(data_.product.empty() || data_.version.empty() ||
           data_.guid.empty() || data_.minidump_pathname.empty());
}

bool CastCrashdumpUploader::Upload(std::string* response) {
  if (!http_layer_->Init()) {
    LOG(ERROR) << "http layer Init failed";
    return false;
  }

  if (!CheckRequiredParametersArePresent()) {
    LOG(ERROR) << "Missing required parameters";
    return false;
  }

  struct stat st;
  if (0 != stat(data_.minidump_pathname.c_str(), &st)) {
    LOG(ERROR) << data_.minidump_pathname << " does not exist.";
    return false;
  }

  if (!http_layer_->AddFile(data_.minidump_pathname, "upload_file_minidump")) {
    LOG(ERROR) << "Failed to add file: " << data_.minidump_pathname;
    return false;
  }

  // Populate |parameters_|.
  parameters_[kProdKey] = data_.product;
  parameters_[kVerKey] = data_.version;
  parameters_[kGuidKey] = data_.guid;
  parameters_[kPtimeKey] = data_.ptime;
  parameters_[kCtimeKey] = data_.ctime;
  parameters_[kEmailKey] = data_.email;
  parameters_[kCommentsKey] = data_.comments;

  // Add each attachement in |attachments_|.
  for (auto iter = attachments_.begin(); iter != attachments_.end(); ++iter) {
    // Search for the attachment.
    if (0 != stat(iter->second.c_str(), &st)) {
      LOG(ERROR) << iter->second << " could not be found";
      return false;
    }

    // Add the attachment
    if (!http_layer_->AddFile(iter->second, iter->first)) {
      LOG(ERROR) << "Failed to add file: " << iter->second
                 << " with label: " << iter->first;
      return false;
    }
  }

  LOG(INFO) << "Sending request to " << data_.crash_server;

  int http_status_code;
  std::string http_header_data;
  return http_layer_->SendRequest(data_.crash_server,
                                  parameters_,
                                  &http_status_code,
                                  &http_header_data,
                                  response);
}

void CastCrashdumpUploader::SetParameter(const std::string& key,
                                         const std::string& value) {
  parameters_[key] = value;
}

}  // namespace chromecast
