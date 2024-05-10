// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_security_delegate.h"

#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/security_delegate.h"
#include "components/exo/shell_surface_util.h"
#include "net/base/filename_util.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/file_info.h"

namespace exo::test {

TestSecurityDelegate::TestSecurityDelegate() = default;

TestSecurityDelegate::~TestSecurityDelegate() = default;

bool TestSecurityDelegate::CanSelfActivate(aura::Window* window) const {
  return HasPermissionToActivate(window);
}

bool TestSecurityDelegate::CanLockPointer(aura::Window* window) const {
  return window->GetProperty(chromeos::kUseOverviewToExitPointerLock);
}

exo::SecurityDelegate::SetBoundsPolicy TestSecurityDelegate::CanSetBounds(
    aura::Window* window) const {
  return policy_;
}

std::vector<ui::FileInfo> TestSecurityDelegate::GetFilenames(
    ui::EndpointType source,
    const std::vector<uint8_t>& data) const {
  std::string lines(data.begin(), data.end());
  std::vector<ui::FileInfo> filenames;
  for (std::string_view line : base::SplitStringPiece(
           lines, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path;
    if (net::FileURLToFilePath(GURL(line), &path)) {
      filenames.push_back(ui::FileInfo(std::move(path), base::FilePath()));
    }
  }
  return filenames;
}

void TestSecurityDelegate::SendFileInfo(ui::EndpointType target,
                                        const std::vector<ui::FileInfo>& files,
                                        SendDataCallback callback) const {
  std::vector<std::string> lines;
  for (const auto& file : files) {
    lines.push_back("file://" + file.path.value());
  }
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      base::JoinString(lines, "\r\n")));
}

void TestSecurityDelegate::SendPickle(ui::EndpointType target,
                                      const base::Pickle& pickle,
                                      SendDataCallback callback) {
  send_pickle_callback_ = std::move(callback);
}

void TestSecurityDelegate::SetCanSetBounds(
    exo::SecurityDelegate::SetBoundsPolicy policy) {
  policy_ = policy;
}

void TestSecurityDelegate::RunSendPickleCallback(std::vector<GURL> urls) {
  std::vector<std::string> lines;
  for (const auto& url : urls) {
    lines.push_back(url.spec());
  }
  std::move(send_pickle_callback_)
      .Run(base::MakeRefCounted<base::RefCountedString>(
          base::JoinString(lines, "\r\n")));
}

}  // namespace exo::test
