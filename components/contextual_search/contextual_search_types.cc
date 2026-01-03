// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_types.h"

namespace contextual_search {

FileInfo::FileInfo() = default;
FileInfo::FileInfo(const FileInfo& other) {
  *this = other;
}

FileInfo& FileInfo::operator=(const FileInfo& other) {
  file_token = other.file_token;
  file_name = other.file_name;
  file_size_bytes = other.file_size_bytes;
  selection_time = other.selection_time;
  mime_type = other.mime_type;
  upload_status = other.upload_status;
  upload_error_type = other.upload_error_type;
  tab_url = other.tab_url;
  tab_title = other.tab_title;
  tab_session_id = other.tab_session_id;
  request_id = other.request_id;
  response_bodies = other.response_bodies;
  if (other.input_data) {
    input_data = std::make_unique<lens::ContextualInputData>(*other.input_data);
  } else {
    input_data.reset();
  }
  return *this;
}
FileInfo::~FileInfo() = default;

}  // namespace contextual_search
