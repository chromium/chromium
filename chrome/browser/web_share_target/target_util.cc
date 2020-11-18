// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_share_target/target_util.h"

#include <sstream>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"

namespace {

void AddFile(const std::string& value_name,
             const std::string& file_uri,
             const std::string& file_name,
             const std::string& content_type,
             const std::string& boundary,
             scoped_refptr<network::ResourceRequestBody> result) {
  const char delimiter[] = "\r\n";
  const size_t delimiter_length = 2;
  std::string mime_header;
  // First line is the boundary.
  mime_header.append("--" + boundary + delimiter);
  // Next line is the Content-disposition.
  mime_header.append("Content-Disposition: form-data; name=\"" + value_name +
                     "\"");
  if (!file_name.empty()) {
    mime_header.append("; filename=\"" + file_name + "\"");
  }
  mime_header.append(delimiter);

  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that.
    mime_header.append("Content-Type: " + content_type + delimiter);
  }
  // Leave an empty line before appending the file_uri.
  mime_header.append(delimiter);

  result->AppendBytes(mime_header.c_str(), mime_header.length());

  result->AppendFileRange(
#if defined(OS_WIN)
      base::FilePath::FromUTF8Unsafe(file_uri),
#else
      base::FilePath(file_uri),
#endif
      0, -1, base::Time());

  result->AppendBytes(delimiter, delimiter_length);
}

void AddPlainText(const std::string& value_name,
                  const std::string& value,
                  const std::string& file_name,
                  const std::string& content_type,
                  const std::string& boundary,
                  scoped_refptr<network::ResourceRequestBody> result) {
  std::string item;
  if (file_name.empty()) {
    net::AddMultipartValueForUpload(value_name, value, boundary, content_type,
                                    &item);
  } else {
    net::AddMultipartValueForUploadWithFileName(value_name, file_name, value,
                                                boundary, content_type, &item);
  }
  result->AppendBytes(item.c_str(), item.length());
}

}  // namespace

namespace web_share_target {

std::string PercentEscapeString(const std::string& unescaped_string) {
  std::ostringstream escaped_oss;
  for (char c : unescaped_string) {
    if (c == '"') {
      escaped_oss << "%22";
    } else if (c == 0x0a) {
      escaped_oss << "%0A";
    } else if (c == 0x0d) {
      escaped_oss << "%0D";
    } else {
      escaped_oss << c;
    }
  }
  return escaped_oss.str();
}

scoped_refptr<network::ResourceRequestBody> ComputeMultipartBody(
    const std::vector<std::string>& names,
    const std::vector<std::string>& values,
    const std::vector<bool>& is_value_file_uris,
    const std::vector<std::string>& filenames,
    const std::vector<std::string>& types,
    const std::string& boundary) {
  size_t num_files = names.size();
  if (num_files != values.size() || num_files != is_value_file_uris.size() ||
      num_files != filenames.size() || num_files != types.size()) {
    // The length of all arrays should always be the same for multipart POST.
    // This should never happen.
    return nullptr;
  }
  scoped_refptr<network::ResourceRequestBody> result =
      new network::ResourceRequestBody();

  for (size_t i = 0; i < num_files; i++) {
    if (is_value_file_uris[i]) {
      AddFile(PercentEscapeString(names[i]), values[i],
              PercentEscapeString(filenames[i]), types[i], boundary, result);
    } else {
      AddPlainText(PercentEscapeString(names[i]), values[i],
                   PercentEscapeString(filenames[i]), types[i], boundary,
                   result);
    }
  }

  std::string final_delimiter;
  net::AddMultipartFinalDelimiterForUpload(boundary, &final_delimiter);
  result->AppendBytes(final_delimiter.c_str(), final_delimiter.length());

  return result;
}

std::string ComputeUrlEncodedBody(const std::vector<std::string>& names,
                                  const std::vector<std::string>& values) {
  if (names.size() != values.size() || names.size() == 0)
    return "";
  std::ostringstream application_body_oss;
  application_body_oss << net::EscapeUrlEncodedData(names[0], true) << "="
                       << net::EscapeUrlEncodedData(values[0], true);
  for (size_t i = 1; i < names.size(); i++)
    application_body_oss << "&" << net::EscapeUrlEncodedData(names[i], true)
                         << "=" << net::EscapeUrlEncodedData(values[i], true);

  return application_body_oss.str();
}

}  // namespace web_share_target
