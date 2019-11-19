// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_IME_SERVICE_H_
#define CHROMEOS_SERVICES_IME_IME_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace ime {

class ImeService : public mojom::ImeService,
                   public mojom::InputEngineManager,
                   public ImeCrosPlatform {
 public:
  explicit ImeService(mojo::PendingReceiver<mojom::ImeService> receiver);
  ~ImeService() override;

 private:
  // mojom::ImeService overrides:
  void SetPlatformAccessProvider(
      mojo::PendingRemote<mojom::PlatformAccessProvider> provider) override;
  void BindInputEngineManager(
      mojo::PendingReceiver<mojom::InputEngineManager> receiver) override;

  // mojom::InputEngineManager overrides:
  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override;

  // ImeCrosPlatform overrides:
  const char* GetImeBundleDir() override;
  const char* GetImeGlobalDir() override;
  const char* GetImeUserHomeDir() override;
  int SimpleDownloadToFile(const char* url,
                           const char* file_path,
                           SimpleDownloadCallback callback) override;
  ImeCrosDownloader* GetDownloader() override;
  void RunInMainSequence(ImeSequencedTask task, int task_id) override;

  // Callback used when a file download finishes by the |SimpleURLLoader|.
  // On failure, |file| will be empty.
  void SimpleDownloadFinished(SimpleDownloadCallback callback,
                              const base::FilePath& file);

  mojo::Receiver<mojom::ImeService> receiver_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // For the duration of this service lifetime, there should be only one
  // input engine instance.
  std::unique_ptr<InputEngine> input_engine_;

  // Platform delegate for access to privilege resources.
  mojo::Remote<mojom::PlatformAccessProvider> platform_access_;
  mojo::ReceiverSet<mojom::InputEngineManager> manager_receivers_;

  DISALLOW_COPY_AND_ASSIGN(ImeService);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_IME_SERVICE_H_
