// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/file_handler_info.h"

namespace apps {

namespace file_handler_verbs {

const char kOpenWith[] = "open_with";
const char kAddTo[] = "add_to";
const char kPackWith[] = "pack_with";
const char kShareWith[] = "share_with";

}  // namespace file_handler_verbs

FileHandlerInfo::FileHandlerInfo()
    : include_directories(false), verb(file_handler_verbs::kOpenWith) {}

FileHandlerInfo::FileHandlerInfo(const FileHandlerInfo& other) = default;

FileHandlerInfo::~FileHandlerInfo() = default;

}  // namespace apps
