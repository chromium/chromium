
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_ANCHOVY_PROVIDER_H_
#define COMPONENTS_MANTA_ANCHOVY_PROVIDER_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace manta {

// The Anchovy provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
class COMPONENT_EXPORT(MANTA) AnchovyProvider : public BaseProvider {
 public:
  AnchovyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      bool is_otr_profile_,
      const std::string& chrome_version,
      const std::string& locale);
  AnchovyProvider(const AnchovyProvider&) = delete;
  AnchovyProvider& operator=(const AnchovyProvider&) = delete;
  ~AnchovyProvider() override;

  struct ImageDescriptionRequest {
    ImageDescriptionRequest(std::string source_id,
                            std::string lang_tag,
                            const std::vector<uint8_t>& bytes)
        : image_bytes(bytes), lang_tag(lang_tag), source_id(source_id) {}
    ImageDescriptionRequest(ImageDescriptionRequest&& other) = default;
    ImageDescriptionRequest(const ImageDescriptionRequest&) = delete;
    ImageDescriptionRequest& operator=(const ImageDescriptionRequest&) = delete;
    const raw_ref<const std::vector<uint8_t>> image_bytes;
    const std::string lang_tag;
    const std::string source_id;
  };

  virtual void GetImageDescription(ImageDescriptionRequest& request,
                                   MantaGenericCallback callback);
  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  base::WeakPtrFactory<AnchovyProvider> weak_ptr_factory_{this};
};
}  // namespace manta
#endif  // COMPONENTS_MANTA_ANCHOVY_PROVIDER_H_
