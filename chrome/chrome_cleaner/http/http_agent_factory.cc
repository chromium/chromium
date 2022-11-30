// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/http_agent_factory.h"

#include <memory>

#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/current_module.h"
#include "chrome/chrome_cleaner/http/http_agent_impl.h"

namespace chrome_cleaner {

HttpAgentFactory::~HttpAgentFactory() = default;

std::unique_ptr<chrome_cleaner::HttpAgent> HttpAgentFactory::CreateHttpAgent()
    const {
  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE()));

  DCHECK(file_version_info.get());
  if (file_version_info.get()) {
    return std::make_unique<chrome_cleaner::HttpAgentImpl>(
        base::AsWStringPiece(file_version_info->product_short_name()),
        base::AsWStringPiece(file_version_info->product_version()));
  } else {
    LOG(ERROR) << "Unable to get version string for Chrome Cleanup Tool.";
    return std::make_unique<chrome_cleaner::HttpAgentImpl>(
        L"Chrome Cleanup Tool", L"0.0.99");
  }
}

}  // namespace chrome_cleaner
