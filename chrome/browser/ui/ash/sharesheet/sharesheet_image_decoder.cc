// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_image_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {
std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  base::ReadFileToString(path, &data);
  return data;
}
}  // namespace

constexpr base::TaskTraits kBlockingTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

SharesheetImageDecoder::SharesheetImageDecoder() {}
SharesheetImageDecoder::~SharesheetImageDecoder() = default;

void SharesheetImageDecoder::DecodeImage(apps::mojom::IntentPtr intent,
                                         Profile* profile,
                                         DecodeCallback callback) {
  callback_ = std::move(callback);

  base::FilePath file_paths;
  storage::FileSystemContext* fs_context =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId);

  // Extracts the file path from intent.
  storage::FileSystemURL fs_url =
      fs_context->CrackURL(std::move(intent)->file_urls.value().front());
  file_paths = fs_url.path();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SharesheetImageDecoder::URLToEncodedBytes,
                                weak_ptr_factory_.GetWeakPtr(), file_paths));
}

void SharesheetImageDecoder::URLToEncodedBytes(
    const base::FilePath& image_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kBlockingTaskTraits,
      base::BindOnce(&ReadFileToString, image_path),
      base::BindOnce(&SharesheetImageDecoder::DecodeURLForPreview,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharesheetImageDecoder::DecodeURLForPreview(std::string image_data) {
  // Decode the image in sandboxed process because decode image_data comes from
  // external storage.
  data_decoder::DecodeImageIsolated(
      std::vector<uint8_t>(image_data.begin(), image_data.end()),
      data_decoder::mojom::ImageCodec::kDefault, false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&SharesheetImageDecoder::BitMapToImage,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharesheetImageDecoder::BitMapToImage(const SkBitmap& decoded_image) {
  // Once decoding image process has completed, invoke the specified
  // callback.
  std::move(callback_).Run(
      gfx::Image::CreateFrom1xBitmap(decoded_image).AsImageSkia());
}
