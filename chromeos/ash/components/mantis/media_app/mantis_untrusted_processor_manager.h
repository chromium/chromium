// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_PROCESSOR_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_PROCESSOR_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_processor.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A component that manages the creation and lifetime of a
// `MantisMediaAppUntrustedProcessor`.
// TODO(http://crbug.com/379588392): Rename to  MantisUntrustedServiceManager.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP)
    MantisUntrustedProcessorManager {
 public:
  using CreateResult = media_app_ui::mojom::MantisUntrustedServiceResult;
  using CreateCallback =
      base::OnceCallback<void(mojo::StructPtr<CreateResult>)>;

  MantisUntrustedProcessorManager();
  MantisUntrustedProcessorManager(const MantisUntrustedProcessorManager&) =
      delete;
  MantisUntrustedProcessorManager& operator=(
      const MantisUntrustedProcessorManager&) = delete;
  ~MantisUntrustedProcessorManager();

  void Create(CreateCallback callback);

 private:
  mojo::Remote<mantis::mojom::MantisService> cros_service_;
  std::unique_ptr<MantisMediaAppUntrustedProcessor> mantis_untrusted_processor_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_PROCESSOR_MANAGER_H_
