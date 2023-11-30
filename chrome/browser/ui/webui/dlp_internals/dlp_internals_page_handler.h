// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/ui/webui/dlp_internals/dlp_internals.mojom.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace policy {

// Concrete implementation of dlp_internals::mojom::PageHandler.
class DlpInternalsPageHandler
    : public dlp_internals::mojom::PageHandler,
      public data_controls::DlpReportingManager::Observer {
 public:
  DlpInternalsPageHandler(
      mojo::PendingReceiver<dlp_internals::mojom::PageHandler> receiver,
      Profile* profile);

  DlpInternalsPageHandler(const DlpInternalsPageHandler&) = delete;
  DlpInternalsPageHandler& operator=(const DlpInternalsPageHandler&) = delete;

  ~DlpInternalsPageHandler() override;

 private:
  // dlp_internals::mojom::DlpInternalsPageHandler
  void GetClipboardDataSource(GetClipboardDataSourceCallback callback) override;
  void GetContentRestrictionsInfo(
      GetContentRestrictionsInfoCallback callback) override;
  void ObserveReporting(
      mojo::PendingRemote<dlp_internals::mojom::ReportingObserver> observer)
      override;
  void GetFilesDatabaseEntries(
      GetFilesDatabaseEntriesCallback callback) override;
  void GetFileInode(const std::string& file_name,
                    GetFileInodeCallback callback) override;

  // DlpReportingManager::Observer
  void OnReportEvent(DlpPolicyEvent event) override;

  // The callback for processing database entries retrieved from DlpClient.
  void ProcessDatabaseEntries(GetFilesDatabaseEntriesCallback callback,
                              ::dlp::GetDatabaseEntriesResponse response_proto);

  mojo::Receiver<dlp_internals::mojom::PageHandler> receiver_;
  mojo::RemoteSet<dlp_internals::mojom::ReportingObserver> reporting_observers_;

  base::ScopedObservation<data_controls::DlpReportingManager,
                          data_controls::DlpReportingManager::Observer>
      reporting_observation_{this};

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<DlpInternalsPageHandler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_UI_WEBUI_DLP_INTERNALS_DLP_INTERNALS_PAGE_HANDLER_H_
