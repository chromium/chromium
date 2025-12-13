// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SHARED_CRD_SESSION_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SHARED_CRD_SESSION_WRAPPER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace ash::boca {

class SharedCrdSessionWrapper {
 public:
  SharedCrdSessionWrapper(const SharedCrdSessionWrapper&) = delete;
  SharedCrdSessionWrapper& operator=(const SharedCrdSessionWrapper&) = delete;

  virtual ~SharedCrdSessionWrapper() = default;

  virtual void StartCrdHost(
      const std::string& receiver_email,
      base::OnceCallback<void(const std::string&)> success_callback,
      base::OnceClosure error_callback,
      base::OnceClosure session_finished_callback) = 0;

  virtual void TerminateSession() = 0;

 protected:
  SharedCrdSessionWrapper() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SHARED_CRD_SESSION_WRAPPER_H_
