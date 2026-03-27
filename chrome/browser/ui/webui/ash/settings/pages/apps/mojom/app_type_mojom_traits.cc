// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_type_mojom_traits.h"

namespace mojo {

Readiness EnumTraits<Readiness, apps::Readiness>::ToMojom(
    apps::Readiness input) {
  switch (input) {
    case apps::Readiness::kUnknown:
      return Readiness::kUnknown;
    case apps::Readiness::kReady:
      return Readiness::kReady;
    case apps::Readiness::kDisabledByBlocklist:
      return Readiness::kDisabledByBlocklist;
    case apps::Readiness::kDisabledByPolicy:
      return Readiness::kDisabledByPolicy;
    case apps::Readiness::kDisabledByUser:
      return Readiness::kDisabledByUser;
    case apps::Readiness::kTerminated:
      return Readiness::kTerminated;
    case apps::Readiness::kUninstalledByUser:
      return Readiness::kUninstalledByUser;
    case apps::Readiness::kRemoved:
      return Readiness::kRemoved;
    case apps::Readiness::kUninstalledByNonUser:
      return Readiness::kUninstalledByNonUser;
    case apps::Readiness::kDisabledByLocalSettings:
      return Readiness::kDisabledByLocalSettings;
  }
}

apps::Readiness EnumTraits<Readiness, apps::Readiness>::FromMojom(
    Readiness input) {
  switch (input) {
    case Readiness::kUnknown:
      return apps::Readiness::kUnknown;
    case Readiness::kReady:
      return apps::Readiness::kReady;
    case Readiness::kDisabledByBlocklist:
      return apps::Readiness::kDisabledByBlocklist;
    case Readiness::kDisabledByPolicy:
      return apps::Readiness::kDisabledByPolicy;
    case Readiness::kDisabledByUser:
      return apps::Readiness::kDisabledByUser;
    case Readiness::kTerminated:
      return apps::Readiness::kTerminated;
    case Readiness::kUninstalledByUser:
      return apps::Readiness::kUninstalledByUser;
    case Readiness::kRemoved:
      return apps::Readiness::kRemoved;
    case Readiness::kUninstalledByNonUser:
      return apps::Readiness::kUninstalledByNonUser;
    case Readiness::kDisabledByLocalSettings:
      return apps::Readiness::kDisabledByLocalSettings;
  }
}

}  // namespace mojo
