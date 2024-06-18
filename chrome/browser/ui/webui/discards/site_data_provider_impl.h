// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_SITE_DATA_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_SITE_DATA_PROVIDER_IMPL_H_

#include <memory>

#include "base/sequence_checker.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#include "components/performance_manager/public/graph/graph.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace performance_manager {
class Graph;
class SiteDataReader;
}  // namespace performance_manager

class SiteDataProviderImpl : public discards::mojom::SiteDataProvider,
                             public performance_manager::GraphOwnedDefaultImpl {
 public:
  explicit SiteDataProviderImpl(const std::string& profile_id);
  ~SiteDataProviderImpl() override;
  SiteDataProviderImpl(const SiteDataProviderImpl& other) = delete;
  SiteDataProviderImpl& operator=(const SiteDataProviderImpl&) = delete;

  // Creates a new SiteDataProviderImpl to service |receiver| and passes its
  // ownership to |graph|.
  static void CreateAndBind(
      mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver,
      const std::string& profile_id_,
      performance_manager::Graph* graph);

  void GetSiteDataArray(
      const std::vector<std::string>& explicitly_requested_origins,
      GetSiteDataArrayCallback callback) override;
  void GetSiteDataDatabaseSize(
      GetSiteDataDatabaseSizeCallback callback) override;

 private:
  using SiteDataReader = performance_manager::SiteDataReader;
  using OriginToReaderMap =
      base::flat_map<std::string, std::unique_ptr<SiteDataReader>>;

  static void OnConnectionError(SiteDataProviderImpl* impl);

  // Binds |receiver_| by consuming |receiver|, which must be valid.
  void Bind(mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver);

  // This map pins requested readers and their associated data in memory until
  // after the next read finishes. This is necessary to allow the database reads
  // to go through and populate the requested entries.
  OriginToReaderMap requested_origins_;

  std::string profile_id_;

  mojo::Receiver<discards::mojom::SiteDataProvider> receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_SITE_DATA_PROVIDER_IMPL_H_
