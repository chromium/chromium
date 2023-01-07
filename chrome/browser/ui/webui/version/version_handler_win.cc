// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_handler_win.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_version.h"
#include "chrome/browser/ui/webui/version/version_util_win.h"
#include "content/public/browser/web_ui.h"

VersionHandlerWindows::VersionHandlerWindows() {}

VersionHandlerWindows::~VersionHandlerWindows() {}

void VersionHandlerWindows::HandleRequestVersionInfo(
    const base::Value::List& args) {
  VersionHandler::HandleRequestVersionInfo(args);

  // Start the asynchronous load of the versions.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&version_utils::win::GetFullWindowsVersion),
      base::BindOnce(&VersionHandlerWindows::OnVersion,
                     weak_factory_.GetWeakPtr()));
}

void VersionHandlerWindows::OnVersion(const std::string& version) {
  FireWebUIListener("return-os-version", base::Value(version));
}

// static
std::string VersionHandlerWindows::GetFullWindowsVersionForTesting() {
  return version_utils::win::GetFullWindowsVersion();
}
