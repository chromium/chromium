// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_SERVICE_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_SERVICE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/origin_gating/core/checker_id.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_registration.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace origin_gating {

class OriginGatingServiceFactory;

class OriginGatingService : public KeyedService {
 public:
  explicit OriginGatingService(base::PassKey<OriginGatingServiceFactory>);
  OriginGatingService(const OriginGatingService&) = delete;
  OriginGatingService& operator=(const OriginGatingService&) = delete;
  OriginGatingService(OriginGatingService&&) = delete;
  OriginGatingService& operator=(OriginGatingService&&) = delete;
  ~OriginGatingService() override;

  static std::unique_ptr<OriginGatingService> CreateForTesting();

  // Constructs and registers an OriginGatingChecker with the service.
  // The service assumes ownership of the created checker.
  //
  // Callers must not call this after `Shutdown()` has been called.
  [[nodiscard]] std::unique_ptr<OriginGatingRegistration>
  CreateAndRegisterChecker(
      base::WeakPtr<OriginGatingChecker::Delegate> delegate,
      OriginGatingConfiguration config);

  // Unregisters and destroys the OriginGatingChecker associated with `id`.
  void UnregisterChecker(base::PassKey<OriginGatingRegistration>, CheckerId id);

  // Looks up the OriginGatingChecker associated with `id`.
  // Returns nullptr if `id` is null or has been unregistered.
  OriginGatingChecker* GetChecker(CheckerId id) const LIFETIME_BOUND;

  // KeyedService implementation:
  void Shutdown() override;

 private:
  OriginGatingService();

  CheckerId::Generator id_generator_;

  absl::flat_hash_map<CheckerId, std::unique_ptr<OriginGatingChecker>>
      checkers_;

  bool is_shutdown_ = false;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_SERVICE_H_
