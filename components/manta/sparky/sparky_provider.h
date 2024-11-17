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
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/provider_params.h"
#include "components/manta/sparky/sparky_context.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/sparky_util.h"
#include "components/manta/sparky/system_info_delegate.h"
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
      const ProviderParams& provider_params,
      std::unique_ptr<SparkyDelegate> sparky_delegate,
      std::unique_ptr<SystemInfoDelegate> system_info_delegate);

  SparkyProvider(const SparkyProvider&) = delete;
  SparkyProvider& operator=(const SparkyProvider&) = delete;

  ~SparkyProvider() override;

  using SparkyShowAnswerCallback =
      base::OnceCallback<void(MantaStatus, proto::Turn*)>;

  using SparkyProtoResponseCallback =
      base::OnceCallback<void(std::unique_ptr<manta::proto::SparkyResponse>,
                              MantaStatus)>;

  void QuestionAndAnswer(std::unique_ptr<SparkyContext> sparky_context,
                         SparkyShowAnswerCallback done_callback);

  std::vector<manta::FileData> GetFilesSummary();

  // Clears the previous dialogs stored in memory.
  void ClearDialog();

  // Assign the last action as all done to prevent any additional calls to the
  // server.
  void MarkLastActionAllDone();

  int consecutive_assistant_turn_count() {
    return consecutive_assistant_turn_count_;
  }

  bool is_additional_call_expected() { return is_additional_call_expected_; }

 protected:
  SparkyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      std::unique_ptr<SparkyDelegate> sparky_delegate,
      std::unique_ptr<SystemInfoDelegate> system_info_delegate);

 private:
  friend class FakeSparkyProvider;

  // Called if more information is requested from the client. It will make an
  // additional call to QuestionAndAnswer.
  void RequestAdditionalInformation(
      proto::ContextRequest,
      std::unique_ptr<SparkyContext> sparky_context,
      SparkyShowAnswerCallback done_callback,
      manta::MantaStatus status);

  void OnScreenshotObtained(
      std::unique_ptr<SparkyContext> sparky_context,
      SparkyShowAnswerCallback done_callback,
      scoped_refptr<base::RefCountedMemory> png_screenshot);

  void OnResponseReceived(
      SparkyShowAnswerCallback done_callback,
      std::unique_ptr<SparkyContext> sparky_context,
      std::unique_ptr<proto::SparkyResponse> sparky_response,
      manta::MantaStatus status);

  // If the response back is a dialog response with a message to show to the
  // user and potentially actions.
  void OnDialogResponse(std::unique_ptr<SparkyContext> sparky_context,
                        proto::Turn latest_reply,
                        SparkyShowAnswerCallback done_callback,
                        manta::MantaStatus status);

  void OnStorageReceived(std::unique_ptr<SparkyContext> sparky_context,
                         SparkyShowAnswerCallback done_callback,
                         manta::MantaStatus status,
                         std::vector<Diagnostics> diagnostics_vector,
                         std::unique_ptr<StorageData> storage_data);

  void OnFilesObtained(std::unique_ptr<SparkyContext> sparky_context,
                       SparkyShowAnswerCallback done_callback,
                       manta::MantaStatus status,
                       std::vector<FileData> files_data);

  void OnDiagnosticsReceived(std::unique_ptr<SparkyContext> sparky_context,
                             SparkyShowAnswerCallback done_callback,
                             manta::MantaStatus status,
                             std::unique_ptr<StorageData> storage_data,
                             std::unique_ptr<DiagnosticsData> diagnostics_data);

  // Stores the dialog of the question and answers along with any associated
  // actions.
  proto::Request request_;

  // Number of consecutive assistant turns at the end of `request_`.
  int consecutive_assistant_turn_count_ = 0;

  // Boolean indicate if an extra server request is required. It's updated
  // whenever a new agent response is received, the turn limit is reached or a
  // new conversation is started.
  bool is_additional_call_expected_ = false;

  std::unique_ptr<SparkyDelegate> sparky_delegate_;
  std::unique_ptr<SystemInfoDelegate> system_info_delegate_;
  base::WeakPtrFactory<SparkyProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SPARKY_SPARKY_PROVIDER_H_
