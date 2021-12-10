// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory.h"

#include "base/no_destructor.h"
#include "chromeos/services/cros_healthd/private/cpp/internal_service_factory_impl.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {

InternalServiceFactory* InternalServiceFactory::GetInstance() {
  static base::NoDestructor<InternalServiceFactoryImpl> service_factory;
  return service_factory.get();
}

}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
