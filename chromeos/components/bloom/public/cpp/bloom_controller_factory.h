// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_CONTROLLER_FACTORY_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_CONTROLLER_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {
namespace bloom {

class BloomController;

class COMPONENT_EXPORT(BLOOM) BloomControllerFactory {
 public:
  // Create the Bloom controller. Can only be invoked once.
  static std::unique_ptr<BloomController> Create(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory,
      signin::IdentityManager* identity_manager);
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_CONTROLLER_FACTORY_H_
