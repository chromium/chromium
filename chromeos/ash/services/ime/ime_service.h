// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_IME_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_IME_IME_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/ime/decoder/decoder_engine.h"
#include "chromeos/ash/services/ime/decoder/system_engine.h"
#include "chromeos/ash/services/ime/input_method_user_data_service_impl.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/mojom/ime_service.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {
namespace ime {

class FieldTrialParamsRetriever {
 public:
  virtual ~FieldTrialParamsRetriever() = default;

  virtual std::string GetFieldTrialParamValueByFeature(
      const base::Feature& feature,
      const std::string& param_name) = 0;
};

class FieldTrialParamsRetrieverImpl : public FieldTrialParamsRetriever {
 public:
  explicit FieldTrialParamsRetrieverImpl() = default;
  ~FieldTrialParamsRetrieverImpl() override = default;
  FieldTrialParamsRetrieverImpl(const FieldTrialParamsRetrieverImpl&) = delete;
  FieldTrialParamsRetrieverImpl& operator=(
      const FieldTrialParamsRetrieverImpl&) = delete;

  std::string GetFieldTrialParamValueByFeature(
      const base::Feature& feature,
      const std::string& param_name) override;
};

class ImeService : public mojom::ImeService,
                   public mojom::InputEngineManager,
                   public ImeCrosPlatform {
 public:
  explicit ImeService(
      mojo::PendingReceiver<mojom::ImeService> receiver,
      ImeSharedLibraryWrapper* ime_decoder,
      std::unique_ptr<FieldTrialParamsRetriever> field_trial_params_retriever);

  ImeService(const ImeService&) = delete;
  ImeService& operator=(const ImeService&) = delete;

  ~ImeService() override;

  // ImeCrosPlatform overrides:
  const char* GetFieldTrialParamValueByFeature(const char* feature_name,
                                               const char* param_name) override;

 private:
  // mojom::ImeService overrides:
  void SetPlatformAccessProvider(
      mojo::PendingRemote<mojom::PlatformAccessProvider> provider) override;
  void BindInputEngineManager(
      mojo::PendingReceiver<mojom::InputEngineManager> receiver) override;
  void BindInputMethodUserDataService(
      mojo::PendingReceiver<mojom::InputMethodUserDataService> receiver)
      override;

  // mojom::InputEngineManager overrides:
  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override;
  void InitializeConnectionFactory(
      mojo::PendingReceiver<mojom::ConnectionFactory> connection_factory,
      InitializeConnectionFactoryCallback callback) override;

  // ImeCrosPlatform overrides:
  const char* GetImeBundleDir() override;
  const char* GetImeUserHomeDir() override;
  void Unused3() override;
  void Unused2() override;
  int SimpleDownloadToFileV2(const char* url,
                             const char* file_path,
                             SimpleDownloadCallbackV2 callback) override;
  void Unused1() override;
  void RunInMainSequence(ImeSequencedTask task, int task_id) override;
  bool IsFeatureEnabled(const char* feature_name) override;

  // Callback used when a file download finishes by the |SimpleURLLoader|.
  // The |url| is the original download url and bound when downloading request
  // starts. On failure, |file| will be empty.
  void SimpleDownloadFinishedV2(SimpleDownloadCallbackV2 callback,
                                const std::string& url_str,
                                const base::FilePath& file);
  const void* Unused4() override;
  const MojoSystemThunks2* GetMojoSystemThunks2() override;

  // To be called before attempting to initialise a new backend connection, to
  // ensure there is one and only one such connection at any point in time.
  void ResetAllBackendConnections();

  mojo::Receiver<mojom::ImeService> receiver_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // For the duration of this ImeService's lifetime, there should be one and
  // only one of these backend connections (represented as "engine" instances)
  // at any point in time.
  std::unique_ptr<DecoderEngine> proto_mode_shared_lib_engine_;
  std::unique_ptr<SystemEngine> mojo_mode_shared_lib_engine_;

  // InputMethodUserDataService runs independently of the DecoderEngine and
  // SystemEngine, and is always connected after binding.
  std::unique_ptr<InputMethodUserDataServiceImpl> input_method_user_data_api_;

  // Platform delegate for access to privilege resources.
  mojo::Remote<mojom::PlatformAccessProvider> platform_access_;
  mojo::ReceiverSet<mojom::InputEngineManager> manager_receivers_;

  raw_ptr<ImeSharedLibraryWrapper> ime_shared_library_ = nullptr;

  std::unique_ptr<FieldTrialParamsRetriever> field_trial_params_retriever_;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_IME_SERVICE_H_
