// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"

VersionHandlerChromeOS::VersionHandlerChromeOS() {}

VersionHandlerChromeOS::~VersionHandlerChromeOS() {}

void VersionHandlerChromeOS::HandleRequestVersionInfo(
    const base::ListValue* args) {
  VersionHandler::HandleRequestVersionInfo(args);

  // Start the asynchronous load of the versions.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&VersionHandlerChromeOS::OnVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetFirmware),
      base::BindOnce(&VersionHandlerChromeOS::OnOSFirmware,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetARCVersion),
      base::BindOnce(&VersionHandlerChromeOS::OnARCVersion,
                     weak_factory_.GetWeakPtr()));
}

void VersionHandlerChromeOS::OnVersion(const std::string& version) {
  FireWebUIListener("return-os-version", base::Value(version));
}

void VersionHandlerChromeOS::OnOSFirmware(const std::string& version) {
  FireWebUIListener("return-os-firmware-version", base::Value(version));
}

void VersionHandlerChromeOS::OnARCVersion(const std::string& version) {
  FireWebUIListener("return-arc-version", base::Value(version));
}
