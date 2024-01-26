// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
#define CHROME_UTILITY_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

class GURL;
struct ImportedBookmarkEntry;
struct ImporterURLRow;

namespace importer {
struct SearchEngineInfo;
}

// TODO(tibell): Now that profile import is a Mojo service perhaps ImportBridge,
// ProfileWriter or something in between should be the actual Mojo interface,
// instead of having the current split.

// When the importer is run in an external process, the bridge is effectively
// split in half by the IPC infrastructure.  The external bridge receives data
// and notifications from the importer, and sends it across IPC.  The
// internal bridge gathers the data from the IPC host and writes it to the
// profile.
class ExternalProcessImporterBridge : public ImporterBridge {
 public:
  // |observer| must outlive this object.
  ExternalProcessImporterBridge(
      const base::flat_map<uint32_t, std::string>& localized_strings,
      mojo::SharedRemote<chrome::mojom::ProfileImportObserver> observer);

  ExternalProcessImporterBridge(const ExternalProcessImporterBridge&) = delete;
  ExternalProcessImporterBridge& operator=(
      const ExternalProcessImporterBridge&) = delete;

  // Begin ImporterBridge implementation:
  void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                    const std::u16string& first_folder_name) override;

  void AddHomePage(const GURL& home_page) override;

  void SetFavicons(const favicon_base::FaviconUsageDataList& favicons) override;

  void SetHistoryItems(const std::vector<ImporterURLRow>& rows,
                       importer::VisitSource visit_source) override;

  void SetKeywords(
      const std::vector<importer::SearchEngineInfo>& search_engines,
      bool unique_on_host_and_path) override;

  void SetPasswordForm(const importer::ImportedPasswordForm& form) override;

  void SetAutofillFormData(
      const std::vector<ImporterAutofillFormDataEntry>& entries) override;

  void NotifyStarted() override;
  void NotifyItemStarted(importer::ImportItem item) override;
  void NotifyItemEnded(importer::ImportItem item) override;
  void NotifyEnded() override;

  std::u16string GetLocalizedString(int message_id) override;
  // End ImporterBridge implementation.

 private:
  ~ExternalProcessImporterBridge() override;

  // Holds strings needed by the external importer because the resource
  // bundle isn't available to the external process.
  base::flat_map<uint32_t, std::string> localized_strings_;

  mojo::SharedRemote<chrome::mojom::ProfileImportObserver> observer_;
};

#endif  // CHROME_UTILITY_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
