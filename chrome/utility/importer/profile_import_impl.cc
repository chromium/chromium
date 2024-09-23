// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/profile_import_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/utility/importer/external_process_importer_bridge.h"
#include "chrome/utility/importer/importer.h"
#include "chrome/utility/importer/importer_creator.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

ProfileImportImpl::ProfileImportImpl(
    mojo::PendingReceiver<chrome::mojom::ProfileImport> receiver)
    : receiver_(this, std::move(receiver)) {}

ProfileImportImpl::~ProfileImportImpl() = default;

void ProfileImportImpl::StartImport(
    const importer::SourceProfile& source_profile,
    uint16_t items,
    const base::flat_map<uint32_t, std::string>& localized_strings,
    mojo::PendingRemote<chrome::mojom::ProfileImportObserver> observer) {
  content::UtilityThread::Get()->EnsureBlinkInitialized();
  importer_ = importer::CreateImporterByType(source_profile.importer_type);
  if (!importer_.get()) {
    mojo::Remote<chrome::mojom::ProfileImportObserver>(std::move(observer))
        ->OnImportFinished(false, "Importer could not be created.");
    return;
  }

  items_to_import_ = items;

  // Create worker thread in which importer runs.
  import_thread_ = std::make_unique<base::Thread>("import_thread");
#if BUILDFLAG(IS_WIN)
  import_thread_->init_com_with_mta(false);
#endif
  if (!import_thread_->Start()) {
    NOTREACHED_IN_MIGRATION();
    ImporterCleanup();
  }
  bridge_ = new ExternalProcessImporterBridge(
      localized_strings,
      mojo::SharedRemote<chrome::mojom::ProfileImportObserver>(
          std::move(observer)));
  import_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Importer::StartImport, importer_, source_profile, items,
                     base::RetainedRef(bridge_)));
}

void ProfileImportImpl::CancelImport() {
  ImporterCleanup();
}

void ProfileImportImpl::ReportImportItemFinished(importer::ImportItem item) {
  items_to_import_ ^= item;  // Remove finished item from mask.
  if (items_to_import_ == 0) {
    ImporterCleanup();
  }
}

void ProfileImportImpl::ImporterCleanup() {
  importer_->Cancel();
  importer_.reset();
  bridge_.reset();
  import_thread_.reset();
}
