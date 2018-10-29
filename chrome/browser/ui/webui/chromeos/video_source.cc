// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/video_source.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/url_constants.h"
#include "net/base/mime_util.h"

namespace chromeos {
namespace {

const char kWhitelistedDirectory[] = "oobe_videos";

bool IsWhitelisted(const std::string& path) {
  base::FilePath file_path(path);
  if (file_path.ReferencesParent())
    return false;

  // Check if the path starts with a whitelisted directory.
  std::vector<std::string> components;
  file_path.GetComponents(&components);
  if (components.empty())
    return false;
  return components[0] == kWhitelistedDirectory;
}

// Callback for user_manager::UserImageLoader.
void VideoLoaded(
    const content::URLDataSource::GotDataCallback& got_data_callback,
    std::unique_ptr<std::string> video_data,
    bool did_load_file) {
  if (video_data->size() && did_load_file) {
    got_data_callback.Run(new base::RefCountedBytes(
        reinterpret_cast<const unsigned char*>(video_data->data()),
        video_data->size()));
  } else {
    got_data_callback.Run(nullptr);
  }
}

}  // namespace

VideoSource::VideoSource() : weak_factory_(this) {
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

VideoSource::~VideoSource() {}

std::string VideoSource::GetSource() const {
  return chrome::kChromeOSAssetHost;
}

void VideoSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& got_data_callback) {
  if (!IsWhitelisted(path)) {
    got_data_callback.Run(nullptr);
    return;
  }

  const base::FilePath asset_dir(chrome::kChromeOSAssetPath);
  const base::FilePath video_path = asset_dir.AppendASCII(path);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::PathExists, video_path),
      base::BindOnce(&VideoSource::StartDataRequestAfterPathExists,
                     weak_factory_.GetWeakPtr(), video_path,
                     got_data_callback));
}

std::string VideoSource::GetMimeType(const std::string& path) const {
  std::string mime_type;
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

void VideoSource::StartDataRequestAfterPathExists(
    const base::FilePath& video_path,
    const content::URLDataSource::GotDataCallback& got_data_callback,
    bool path_exists) {
  if (path_exists) {
    auto video_data = std::make_unique<std::string>();
    std::string* data = video_data.get();
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&base::ReadFileToString, video_path, data),
        base::BindOnce(&VideoLoaded, got_data_callback, std::move(video_data)));

  } else {
    got_data_callback.Run(nullptr);
  }
}

}  // namespace chromeos
