// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_STORAGE_ADAPTER_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_STORAGE_ADAPTER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/cdm_factory_daemon/mojom/cdm_storage.mojom.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// This class adapts the chromeos::cdm::mojom::CdmStorage interface to the
// media::mojom::CdmStorage and media::mojom::CdmFile interfaces. The incoming
// chromeos::cdm::mojom::CdmStorage interface has all methods marked Sync which
// means we will never receive simultaneous calls.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) CdmStorageAdapter
    : public cdm::mojom::CdmStorage {
 public:
  CdmStorageAdapter(
      media::mojom::FrameInterfaceFactory* frame_interfaces,
      mojo::PendingAssociatedReceiver<cdm::mojom::CdmStorage> receiver);

  CdmStorageAdapter(const CdmStorageAdapter&) = delete;
  CdmStorageAdapter& operator=(const CdmStorageAdapter&) = delete;

  ~CdmStorageAdapter() override;

  // chromeos::cdm::mojom::CdmStorage:
  void Read(const std::string& file_name, ReadCallback callback) override;
  void Write(const std::string& file_name,
             const std::vector<uint8_t>& data,
             WriteCallback callback) override;
  void Exists(const std::string& file_name, ExistsCallback callback) override;
  void GetSize(const std::string& file_name, GetSizeCallback callback) override;
  void Remove(const std::string& file_name, RemoveCallback callback) override;

 private:
  void OnOpenForRead(
      ReadCallback callback,
      media::mojom::CdmStorage::Status status,
      mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file);
  void OnReadComplete(ReadCallback callback,
                      media::mojom::CdmFile::Status status,
                      const std::vector<uint8_t>& data);
  void OnOpenForWrite(
      const std::vector<uint8_t>& data,
      WriteCallback callback,
      media::mojom::CdmStorage::Status status,
      mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file);
  void OnWriteComplete(WriteCallback callback,
                       media::mojom::CdmFile::Status status);

  mojo::AssociatedReceiver<cdm::mojom::CdmStorage> receiver_;
  mojo::Remote<media::mojom::CdmStorage> cdm_storage_remote_;

  // |cdm_file_| is used to read and write the file and is released when the
  // file is closed so that CdmStorage can tell that the file is no longer being
  // used. It's safe to reuse this because our incoming mojo interface is
  // defined as all Sync methods so we will never receive two calls at once.
  mojo::AssociatedRemote<media::mojom::CdmFile> cdm_file_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<CdmStorageAdapter> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_STORAGE_ADAPTER_H_
