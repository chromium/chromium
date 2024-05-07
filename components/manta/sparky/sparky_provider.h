// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SPARKY_SPARKY_PROVIDER_H_
#define COMPONENTS_MANTA_SPARKY_SPARKY_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace manta {

// The Sparky provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
// IMPORTANT: This class depends on `IdentityManager`.
// `SparkyProvider::Call` will return an empty response after `IdentityManager`
// destruction.
class COMPONENT_EXPORT(MANTA) SparkyProvider : virtual public BaseProvider {
 public:
  // Returns a `SparkyProvider` instance tied to the profile of the passed
  // arguments.
  SparkyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      bool is_demo_mode,
      const std::string& chrome_version,
      std::unique_ptr<SparkyDelegate> sparky_delegate);

  SparkyProvider(const SparkyProvider&) = delete;
  SparkyProvider& operator=(const SparkyProvider&) = delete;

  ~SparkyProvider() override;

  // TODO Update this with Sparky information
  using SparkyQAPair = std::pair<std::string, std::string>;

  using SparkyShowAnswerCallback =
      base::OnceCallback<void(const std::string&, MantaStatus)>;

  void QuestionAndAnswer(const std::string& content,
                         const std::vector<SparkyQAPair> QAHistory,
                         const std::string& question,
                         proto::Task task,
                         SparkyShowAnswerCallback done_callback);

 protected:
  SparkyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      std::unique_ptr<SparkyDelegate> sparky_delegate);

 private:
  friend class FakeSparkyProvider;

  // Called if more information is requested from the client. It will make an
  // additional call to QuestionAndAnswer.
  void RequestAdditionalInformation(proto::ContextRequest,
                                    const std::string& original_content,
                                    const std::vector<SparkyQAPair> QAHistory,
                                    const std::string& question,
                                    SparkyShowAnswerCallback done_callback,
                                    manta::MantaStatus status);

  void OnResponseReceived(SparkyShowAnswerCallback done_callback,
                          const std::string& original_content,
                          const std::vector<SparkyQAPair> QAHistory,
                          const std::string& question,
                          std::unique_ptr<proto::Response> output_data,
                          manta::MantaStatus status);

  void UpdateSettings(proto::SettingsData);

  // If the response back is the final response to show to the user.
  void OnActionResponse(proto::FinalResponse,
                        SparkyShowAnswerCallback done_callback,
                        manta::MantaStatus status);

  std::unique_ptr<SparkyDelegate> sparky_delegate_;

  base::WeakPtrFactory<SparkyProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SPARKY_SPARKY_PROVIDER_H_
