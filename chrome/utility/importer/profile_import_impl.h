// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_PROFILE_IMPORT_IMPL_H_
#define CHROME_UTILITY_IMPORTER_PROFILE_IMPORT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ExternalProcessImporterBridge;
class Importer;

namespace base {
class Thread;
}  // namespace base

namespace importer {
struct SourceProfile;
}

class ProfileImportImpl : public chrome::mojom::ProfileImport {
 public:
  explicit ProfileImportImpl(
      mojo::PendingReceiver<chrome::mojom::ProfileImport> receiver);

  ProfileImportImpl(const ProfileImportImpl&) = delete;
  ProfileImportImpl& operator=(const ProfileImportImpl&) = delete;

  ~ProfileImportImpl() override;

 private:
  // chrome::mojom::ProfileImport:
  void StartImport(
      const importer::SourceProfile& source_profile,
      uint16_t items,
      const base::flat_map<uint32_t, std::string>& localized_strings,
      mojo::PendingRemote<chrome::mojom::ProfileImportObserver> observer)
      override;
  void CancelImport() override;
  void ReportImportItemFinished(importer::ImportItem item) override;

  // The following are used with out of process profile import:
  void ImporterCleanup();

  mojo::Receiver<chrome::mojom::ProfileImport> receiver_;

  // Thread that importer runs on, while ProfileImportThread handles messages
  // from the browser process.
  std::unique_ptr<base::Thread> import_thread_;

  // Bridge object is passed to importer, so that it can send IPC calls
  // directly back to the ProfileImportProcessHost.
  scoped_refptr<ExternalProcessImporterBridge> bridge_;

  // A bitmask of importer::ImportItem.
  uint16_t items_to_import_ = 0;

  // Importer of the appropriate type (Firefox, Safari, IE, etc.)
  scoped_refptr<Importer> importer_;
};

#endif  // CHROME_UTILITY_IMPORTER_PROFILE_IMPORT_IMPL_H_
