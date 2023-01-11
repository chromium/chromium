// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/cdm_storage_adapter.h"

#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace chromeos {

namespace {

// Helper function to adapt Exists using Read since CdmStorage doesn't have an
// Exists implementation. We test it by seeing if the file is non-empty.
void ReadToExists(cdm::mojom::CdmStorage::ExistsCallback callback,
                  bool success,
                  const std::vector<uint8_t>& data) {
  std::move(callback).Run(success && !data.empty());
}

// Helper function to adapt GetSize using Read since CdmFile doesn't have a
// GetSize implementation. We get the size by reading the file contents and
// determining the length. These files will be small in size and are accessed
// infrequently, so this isn't that much of a penalty to pay.
void ReadToGetSize(cdm::mojom::CdmStorage::GetSizeCallback callback,
                   bool success,
                   const std::vector<uint8_t>& data) {
  std::move(callback).Run(success, data.size());
}

}  // namespace

CdmStorageAdapter::CdmStorageAdapter(
    media::mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingAssociatedReceiver<chromeos::cdm::mojom::CdmStorage> receiver)
    : receiver_(this, std::move(receiver)) {
  CHECK(frame_interfaces);
  frame_interfaces->CreateCdmStorage(
      cdm_storage_remote_.BindNewPipeAndPassReceiver());
}

CdmStorageAdapter::~CdmStorageAdapter() = default;

void CdmStorageAdapter::Read(const std::string& file_name,
                             ReadCallback callback) {
  DVLOG(1) << "Read " << file_name;
  CHECK(!cdm_file_);
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), false, std::vector<uint8_t>());
  cdm_storage_remote_->Open(
      file_name,
      base::BindOnce(&CdmStorageAdapter::OnOpenForRead,
                     weak_factory_.GetWeakPtr(), std::move(wrapped_callback)));
}

void CdmStorageAdapter::OnOpenForRead(
    ReadCallback callback,
    media::mojom::CdmStorage::Status status,
    mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file) {
  if (status != media::mojom::CdmStorage::Status::kSuccess) {
    std::move(callback).Run(false, {});
    return;
  }
  cdm_file_.Bind(std::move(cdm_file));
  cdm_file_->Read(base::BindOnce(&CdmStorageAdapter::OnReadComplete,
                                 weak_factory_.GetWeakPtr(),
                                 std::move(callback)));
}

void CdmStorageAdapter::OnReadComplete(ReadCallback callback,
                                       media::mojom::CdmFile::Status status,
                                       const std::vector<uint8_t>& data) {
  cdm_file_.reset();
  if (status != media::mojom::CdmFile::Status::kSuccess) {
    std::move(callback).Run(false, {});
    return;
  }
  std::move(callback).Run(true, data);
}

void CdmStorageAdapter::Write(const std::string& file_name,
                              const std::vector<uint8_t>& data,
                              WriteCallback callback) {
  DVLOG(1) << "Write " << file_name;
  CHECK(!cdm_file_);
  auto wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);
  cdm_storage_remote_->Open(file_name,
                            base::BindOnce(&CdmStorageAdapter::OnOpenForWrite,
                                           weak_factory_.GetWeakPtr(), data,
                                           std::move(wrapped_callback)));
}

void CdmStorageAdapter::OnOpenForWrite(
    const std::vector<uint8_t>& data,
    WriteCallback callback,
    media::mojom::CdmStorage::Status status,
    mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file) {
  if (status != media::mojom::CdmStorage::Status::kSuccess) {
    std::move(callback).Run(false);
    return;
  }
  cdm_file_.Bind(std::move(cdm_file));
  cdm_file_->Write(
      data, base::BindOnce(&CdmStorageAdapter::OnWriteComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CdmStorageAdapter::OnWriteComplete(WriteCallback callback,
                                        media::mojom::CdmFile::Status status) {
  cdm_file_.reset();
  std::move(callback).Run(status == media::mojom::CdmFile::Status::kSuccess);
}

void CdmStorageAdapter::Exists(const std::string& file_name,
                               ExistsCallback callback) {
  DVLOG(1) << "Exists " << file_name;
  Read(file_name, base::BindOnce(&ReadToExists, std::move(callback)));
}

void CdmStorageAdapter::GetSize(const std::string& file_name,
                                GetSizeCallback callback) {
  DVLOG(1) << "GetSize " << file_name;
  Read(file_name, base::BindOnce(&ReadToGetSize, std::move(callback)));
}

void CdmStorageAdapter::Remove(const std::string& file_name,
                               RemoveCallback callback) {
  DVLOG(1) << "Remove " << file_name;
  // Writing a zero length array to the file removes it.
  Write(file_name, {}, std::move(callback));
}

}  // namespace chromeos