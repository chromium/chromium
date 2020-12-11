// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_POPULAR_SITES_IMPL_H_
#define COMPONENTS_NTP_TILES_POPULAR_SITES_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/ntp_tiles/popular_sites.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace variations {
class VariationsService;
}

class PrefService;
class TemplateURLService;

namespace ntp_tiles {

// Actual (non-test) implementation of the PopularSites interface. Caches the
// downloaded file on disk to avoid re-downloading on every startup.
class PopularSitesImpl : public PopularSites {
 public:
  PopularSitesImpl(
      PrefService* prefs,
      const TemplateURLService* template_url_service,
      variations::VariationsService* variations_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~PopularSitesImpl() override;

  // PopularSites implementation.
  bool MaybeStartFetch(bool force_download, FinishedCallback callback) override;
  const std::map<SectionType, SitesVector>& sections() const override;
  GURL GetLastURLFetched() const override;
  GURL GetURLToFetch() override;
  std::string GetDirectoryToFetch() override;
  std::string GetCountryToFetch() override;
  std::string GetVersionToFetch() override;
  const base::ListValue* GetCachedJson() override;

  // Register preferences used by this class.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

 private:
  // Fetch the popular sites at the given URL, overwriting any cache in prefs
  // that already exists.
  void FetchPopularSites();

  // Called once SimpleURLLoader completes the network request.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnDownloadFailed();

  // Parameters set from constructor.
  PrefService* const prefs_;
  const TemplateURLService* const template_url_service_;
  variations::VariationsService* const variations_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Set by MaybeStartFetch() and called after fetch completes.
  FinishedCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  bool is_fallback_;
  std::map<SectionType, SitesVector> sections_;
  GURL pending_url_;
  int version_in_pending_url_;

  base::WeakPtrFactory<PopularSitesImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PopularSitesImpl);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_POPULAR_SITES_IMPL_H_
