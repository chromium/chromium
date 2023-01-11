// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_data_exchange_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace exo {

TestDataExchangeDelegate::TestDataExchangeDelegate() = default;

TestDataExchangeDelegate::~TestDataExchangeDelegate() = default;

ui::EndpointType TestDataExchangeDelegate::GetDataTransferEndpointType(
    aura::Window* window) const {
  return endpoint_type_;
}

std::vector<ui::FileInfo> TestDataExchangeDelegate::GetFilenames(
    ui::EndpointType source,
    const std::vector<uint8_t>& data) const {
  std::string lines(data.begin(), data.end());
  std::vector<ui::FileInfo> filenames;
  for (const base::StringPiece& line : base::SplitStringPiece(
           lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path;
    if (net::FileURLToFilePath(GURL(line), &path))
      filenames.push_back(ui::FileInfo(std::move(path), base::FilePath()));
  }
  return filenames;
}

std::string TestDataExchangeDelegate::GetMimeTypeForUriList(
    ui::EndpointType target) const {
  return "text/uri-list";
}

void TestDataExchangeDelegate::SendFileInfo(
    ui::EndpointType target,
    const std::vector<ui::FileInfo>& files,
    SendDataCallback callback) const {
  std::vector<std::string> lines;
  for (const auto& file : files) {
    lines.push_back("file://" + file.path.value());
  }
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      base::JoinString(lines, "\r\n")));
}

bool TestDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  return true;
}

void TestDataExchangeDelegate::SendPickle(ui::EndpointType target,
                                          const base::Pickle& pickle,
                                          SendDataCallback callback) {
  send_pickle_callback_ = std::move(callback);
}

void TestDataExchangeDelegate::RunSendPickleCallback(std::vector<GURL> urls) {
  std::vector<std::string> lines;
  for (const auto& url : urls) {
    lines.push_back(url.spec());
  }
  std::move(send_pickle_callback_)
      .Run(base::MakeRefCounted<base::RefCountedString>(
          base::JoinString(lines, "\r\n")));
}

std::vector<ui::FileInfo> TestDataExchangeDelegate::ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle) const {
  std::vector<ui::FileInfo> file_info;
  std::string lines(static_cast<const char*>(pickle.data()), pickle.size());
  for (const base::StringPiece& line : base::SplitStringPiece(
           lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path;
    if (net::FileURLToFilePath(GURL(line), &path))
      file_info.push_back(ui::FileInfo(std::move(path), base::FilePath()));
  }
  return file_info;
}

TestDataSourceDelegate::TestDataSourceDelegate() = default;
TestDataSourceDelegate::~TestDataSourceDelegate() = default;

void TestDataSourceDelegate::OnSend(const std::string& mime_type,
                                    base::ScopedFD fd) {
  constexpr char kText[] = "test";
  if (data_map_.empty()) {
    base::WriteFileDescriptor(fd.get(), kText);
  } else {
    base::WriteFileDescriptor(fd.get(), data_map_[mime_type]);
  }
}

void TestDataSourceDelegate::OnCancelled() {
  cancelled_ = true;
}

void TestDataSourceDelegate::OnDndFinished() {
  finished_ = true;
}

bool TestDataSourceDelegate::CanAcceptDataEventsForSurface(
    Surface* surface) const {
  return true;
}

void TestDataSourceDelegate::SetData(const std::string& mime_type,
                                     std::vector<uint8_t> data) {
  data_map_[mime_type] = std::move(data);
}

}  // namespace exo
