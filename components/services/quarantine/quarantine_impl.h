// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_IMPL_H_
#define COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_IMPL_H_

#include <memory>

#include "build/build_config.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif  // BUILDFLAG(IS_WIN)

namespace quarantine {

class QuarantineImpl : public mojom::Quarantine {
 public:
  QuarantineImpl();
  explicit QuarantineImpl(mojo::PendingReceiver<mojom::Quarantine> receiver);

  QuarantineImpl(const QuarantineImpl&) = delete;
  QuarantineImpl& operator=(const QuarantineImpl&) = delete;

  ~QuarantineImpl() override;

  // mojom::Quarantine:
  void QuarantineFile(
      const base::FilePath& full_path,
      const GURL& source_url,
      const GURL& referrer_url,
      const std::optional<url::Origin>& request_initiator,
      const std::string& client_guid,
      mojom::Quarantine::QuarantineFileCallback callback) override;

 private:
  mojo::Receiver<mojom::Quarantine> receiver_{this};

#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::Uninitialization::kBlockPremature};
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_IMPL_H_
