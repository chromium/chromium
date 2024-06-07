// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_thumbnail_loader.h"

#include <optional>

#include "base/files/file_path.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "ipc/ipc_channel.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

std::optional<base::FilePath> MaybeGetDriveFileRelativePath(
    drive::DriveIntegrationService& drive_integration,
    const base::FilePath& path) {
  base::FilePath relative_path;
  if (!drive_integration.GetRelativeDrivePath(path, &relative_path)) {
    return std::nullopt;
  }
  return relative_path;
}

}  // namespace

PickerThumbnailLoader::PickerThumbnailLoader(Profile* profile)
    : profile_(profile), thumbnail_loader_(profile) {}

PickerThumbnailLoader::~PickerThumbnailLoader() = default;

void PickerThumbnailLoader::Load(const base::FilePath& path,
                                 const gfx::Size& size,
                                 LoadCallback callback) {
  // If the file is a drive file, use DriveFS to get the thumbnail.
  if (drive::DriveIntegrationService* drive_integration =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile_)) {
    if (std::optional<base::FilePath> relative_path =
            MaybeGetDriveFileRelativePath(*drive_integration, path);
        relative_path.has_value()) {
      drive_integration->GetThumbnail(
          *relative_path, /*crop_to_square=*/true,
          base::BindOnce(&PickerThumbnailLoader::DecodeDriveThumbnail,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         size));
      return;
    }
  }

  // Fallback to the thumbnail loader extension.
  thumbnail_loader_.Load({path, size}, std::move(callback));
}

void PickerThumbnailLoader::DecodeDriveThumbnail(
    LoadCallback callback,
    const gfx::Size& size,
    const std::optional<std::vector<uint8_t>>& bytes) {
  if (!bytes.has_value()) {
    std::move(callback).Run(nullptr, base::File::Error::FILE_ERROR_FAILED);
    return;
  }

  data_decoder::DecodeImageIsolated(
      *bytes, data_decoder::mojom::ImageCodec::kPng,
      /*shrink_to_fit=*/true, kMaxImageSizeInBytes, size,
      base::BindOnce(&PickerThumbnailLoader::OnDriveThumbnailDecoded,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PickerThumbnailLoader::OnDriveThumbnailDecoded(LoadCallback callback,
                                                    const SkBitmap& image) {
  if (image.empty()) {
    std::move(callback).Run(nullptr, base::File::Error::FILE_ERROR_FAILED);
    return;
  }
  std::move(callback).Run(&image, base::File::Error::FILE_OK);
}
