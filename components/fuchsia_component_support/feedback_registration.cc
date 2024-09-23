// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/feedback_registration.h"

#include <fuchsia/feedback/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <string_view>

#include "base/check.h"
#include "base/fuchsia/process_context.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "components/version_info/version_info.h"

namespace fuchsia_component_support {

namespace {

constexpr char kAbsoluteComponentUrlSchemPrefix[] = "fuchsia-pkg://";

}  // namespace

void RegisterProductDataForCrashReporting(
    std::string_view absolute_component_url,
    std::string_view crash_product_name) {
  DCHECK(base::StartsWith(absolute_component_url,
                          kAbsoluteComponentUrlSchemPrefix));

  fuchsia::feedback::CrashReportingProduct product_data;
  product_data.set_name(std::string(crash_product_name));
  product_data.set_version(std::string(version_info::GetVersionNumber()));
  // TODO(crbug.com/42050100): Use the actual channel when appropriate.
  // For now, always set it to the empty string to avoid reporting "missing".
  product_data.set_channel("");

  auto crash_reporting_service =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::feedback::CrashReportingProductRegister>();

  // Only register the |crash_product_name| for official Chrome-branded builds.
  // Otherwise, the crashes will be handled as non-component-specific crashes.
  // Since Fuchsia handles crashes, it is possible that Fuchsia will upload a
  // crash for an unofficial and/or unbranded build of a Chromium-based
  // component if it is running on an official Fuchsia build. To avoid adding
  // noise from such crash reports, which are not received on other platforms,
  // do not set a product name for such builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD)
  crash_reporting_service->Upsert(std::string(absolute_component_url),
                                  std::move(product_data));
#endif
}

void RegisterProductDataForFeedback(std::string_view component_namespace) {
  fuchsia::feedback::ComponentData component_data;
  component_data.set_namespace_(std::string(component_namespace));
  // TODO(crbug.com/42050100): Add release channel to the annotations.
  component_data.mutable_annotations()->push_back(
      {"version", std::string(version_info::GetVersionNumber())});
  base::ComponentContextForProcess()
      ->svc()
      ->Connect<fuchsia::feedback::ComponentDataRegister>()
      ->Upsert(std::move(component_data), []() {});
}

}  // namespace fuchsia_component_support
