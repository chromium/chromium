// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/download_metadata.h"

namespace {

bool AreResponseHeadersEqual(const net::HttpResponseHeaders* h1,
                             const net::HttpResponseHeaders* h2) {
  if (h1 && h2)
    return h1->raw_headers() == h2->raw_headers();
  return !h1 && !h2;
}

}  // namespace

namespace download {

CompletionInfo::CompletionInfo() = default;

CompletionInfo::CompletionInfo(
    const base::FilePath& path,
    uint64_t bytes_downloaded,
    const std::vector<GURL>& url_chain,
    scoped_refptr<const net::HttpResponseHeaders> response_headers)
    : path(path),
      bytes_downloaded(bytes_downloaded),
      url_chain(url_chain),
      response_headers(std::move(response_headers)) {}

CompletionInfo::CompletionInfo(const base::FilePath& path,
                               uint64_t bytes_downloaded)
    : path(path), bytes_downloaded(bytes_downloaded) {}

CompletionInfo::CompletionInfo(const CompletionInfo& other) = default;

CompletionInfo::~CompletionInfo() = default;

bool CompletionInfo::operator==(const CompletionInfo& other) const {
  // The blob data handle is not compared here.
  return path == other.path && bytes_downloaded == other.bytes_downloaded &&
         url_chain == other.url_chain &&
         AreResponseHeadersEqual(response_headers.get(),
                                 other.response_headers.get()) &&
         hash256 == other.hash256 && custom_data == other.custom_data;
}

DownloadMetaData::DownloadMetaData() : current_size(0u), paused(false) {}

DownloadMetaData::DownloadMetaData(const DownloadMetaData& other) = default;

bool DownloadMetaData::operator==(const DownloadMetaData& other) const {
  return guid == other.guid && current_size == other.current_size &&
         completion_info == other.completion_info && paused == other.paused;
}

DownloadMetaData::~DownloadMetaData() = default;

}  // namespace download
