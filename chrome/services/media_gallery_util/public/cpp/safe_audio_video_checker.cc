// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/cpp/safe_audio_video_checker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "content/public/browser/browser_thread.h"

SafeAudioVideoChecker::SafeAudioVideoChecker(base::File file,
                                             ResultCallback callback)
    : file_(std::move(file)), callback_(std::move(callback)) {
  DCHECK(callback_);
}

void SafeAudioVideoChecker::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!file_.IsValid()) {
    std::move(callback_).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  RetrieveMediaParser();
}

void SafeAudioVideoChecker::OnMediaParserCreated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  static constexpr auto kFileDecodeTime = base::Milliseconds(250);

  media_parser()->CheckMediaFile(
      kFileDecodeTime, std::move(file_),
      base::BindOnce(&SafeAudioVideoChecker::CheckMediaFileDone,
                     base::Unretained(this)));
}

void SafeAudioVideoChecker::OnConnectionError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  CheckMediaFileDone(/*valid=*/false);
}

void SafeAudioVideoChecker::CheckMediaFileDone(bool valid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback_).Run(valid ? base::File::FILE_OK
                                 : base::File::FILE_ERROR_SECURITY);
}

SafeAudioVideoChecker::~SafeAudioVideoChecker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Offload `file_` Close because BrowserThread::IO disallows blocking.
  if (file_.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce([](base::File file) { file.Close(); },
                       std::move(file_)));
  }
}
