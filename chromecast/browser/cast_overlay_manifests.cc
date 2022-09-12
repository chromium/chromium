// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_overlay_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "chromecast/common/mojom/media_caps.mojom.h"
#include "chromecast/common/mojom/memory_pressure.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)
#include "chromecast/external_mojo/broker_service/broker_service.h"  // nogncheck
#endif

#if defined(USE_INTERNAL_OVERLAY_MANIFESTS)
#include "chromecast/internal/shell/browser/cast_content_browser_internal_manifest_overlay.h"
#include "chromecast/internal/shell/browser/cast_content_packaged_services_internal_manifest_overlay.h"
#endif

namespace chromecast {
namespace shell {

const service_manager::Manifest& GetCastContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
                              chromecast::media::mojom::MediaCaps,
                              chromecast::mojom::MemoryPressureController>())
        .Build()
#if defined(USE_INTERNAL_OVERLAY_MANIFESTS)
        .Amend(cast_content_browser_internal_manifest_overlay::GetManifest())
#endif
  };
  return *manifest;
}

const service_manager::Manifest&
GetCastContentPackagedServicesOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
#if BUILDFLAG(ENABLE_EXTERNAL_MOJO_SERVICES)
        .PackageService(chromecast::external_mojo::BrokerService::GetManifest())
#endif
        .Build()
#if defined(USE_INTERNAL_OVERLAY_MANIFESTS)
        .Amend(cast_content_packaged_services_internal_manifest_overlay::
                   GetManifest())
#endif
  };
  return *manifest;
}

}  // namespace shell
}  // namespace chromecast
