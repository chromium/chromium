// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_writer/image_writer_handler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/utility/image_writer/error_message_strings.h"

namespace {

bool IsTestDevice(const base::FilePath& device) {
  return device.AsUTF8Unsafe() ==
         chrome::mojom::RemovableStorageWriter::kTestDevice;
}

base::FilePath MakeTestDevicePath(const base::FilePath& image) {
  return image.ReplaceExtension(FILE_PATH_LITERAL("out"));
}

}  // namespace

namespace image_writer {

ImageWriterHandler::ImageWriterHandler() = default;

ImageWriterHandler::~ImageWriterHandler() = default;

void ImageWriterHandler::Write(
    const base::FilePath& image,
    const base::FilePath& device,
    mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient> client) {
  client_.Bind(std::move(client));
  client_.set_disconnect_handler(
      base::BindOnce(&ImageWriterHandler::Cancel, base::Unretained(this)));

  base::FilePath target_device = device;
  const bool test_mode = IsTestDevice(device);
  if (test_mode)
    target_device = MakeTestDevicePath(image);

  if (ShouldResetImageWriter(image, target_device))
    image_writer_ = std::make_unique<ImageWriter>(this, image, target_device);

  if (image_writer_->IsRunning()) {
    SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  if (test_mode) {
    image_writer_->Write();
    return;
  }

  if (!image_writer_->IsValidDevice()) {
    SendFailed(error::kInvalidDevice);
    return;
  }

  image_writer_->UnmountVolumes(
      base::BindOnce(&ImageWriter::Write, image_writer_->AsWeakPtr()));
}

void ImageWriterHandler::Verify(
    const base::FilePath& image,
    const base::FilePath& device,
    mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient> client) {
  client_.Bind(std::move(client));
  client_.set_disconnect_handler(
      base::BindOnce(&ImageWriterHandler::Cancel, base::Unretained(this)));

  base::FilePath target_device = device;
  const bool test_mode = IsTestDevice(device);
  if (test_mode)
    target_device = MakeTestDevicePath(image);

  if (ShouldResetImageWriter(image, target_device))
    image_writer_ = std::make_unique<ImageWriter>(this, image, target_device);

  if (image_writer_->IsRunning()) {
    SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  if (test_mode) {
    image_writer_->Verify();
    return;
  }

  if (!image_writer_->IsValidDevice()) {
    SendFailed(error::kInvalidDevice);
    return;
  }

  image_writer_->Verify();
}

void ImageWriterHandler::SendProgress(int64_t progress) {
  client_->Progress(progress);
}

void ImageWriterHandler::SendSucceeded() {
  client_->Complete(std::nullopt);
  client_.reset();
}

void ImageWriterHandler::SendFailed(const std::string& error) {
  if (client_) {
    // client_ may be null as the ImageWriter implementation may have reported
    // an error already.
    client_->Complete(error);
    client_.reset();
  }
}

void ImageWriterHandler::Cancel() {
  if (image_writer_)
    image_writer_->Cancel();
  client_.reset();
}

bool ImageWriterHandler::ShouldResetImageWriter(const base::FilePath& image,
                                                const base::FilePath& device) {
  if (!image_writer_)
    return true;
  if (image != image_writer_->GetImagePath())
    return true;
  if (device != image_writer_->GetDevicePath())
    return true;

  // When writing and verifying the same file on the same device, keep
  // the file handles open; do not reset them since that can cause the
  // operation to fail in unexpected ways: crbug.com/352442#c7
  return false;
}

}  // namespace image_writer
