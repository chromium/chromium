// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_REDIRECTION_REDIRECTION_SERVICE_H_
#define CHROME_SERVICES_REDIRECTION_REDIRECTION_SERVICE_H_

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "chrome/services/redirection/public/mojom/redirection_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace redirection {

class COMPONENT_EXPORT(REDIRECTION_SERVICE) RedirectionService final
    : public mojom::RedirectionService {
 public:
  explicit RedirectionService(
      mojo::PendingReceiver<mojom::RedirectionService> receiver);

  RedirectionService(const RedirectionService&) = delete;
  RedirectionService& operator=(const RedirectionService&) = delete;

  ~RedirectionService() override;

 private:
  // mojom::RedirectionService implementation.
  void Start(StartCallback callback) override;

  mojo::Receiver<mojom::RedirectionService> receiver_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace redirection

#endif  // CHROME_SERVICES_REDIRECTION_REDIRECTION_SERVICE_H_
