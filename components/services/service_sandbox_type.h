// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SERVICE_SANDBOX_TYPE_H_
#define COMPONENTS_SERVICES_SERVICE_SANDBOX_TYPE_H_

#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// language_detection::mojom::LanguageDetectionService
namespace language_detection {
namespace mojom {
class LanguageDetectionService;
}
}  // namespace language_detection
template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    language_detection::mojom::LanguageDetectionService>() {
  return sandbox::policy::SandboxType::kUtility;
}

// unzip::mojom::Unzipper
namespace unzip {
namespace mojom {
class Unzipper;
}
}  // namespace unzip
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<unzip::mojom::Unzipper>() {
  return sandbox::policy::SandboxType::kUtility;
}

// patch::mojom::FilePatcher
namespace patch {
namespace mojom {
class FilePatcher;
}
}  // namespace patch
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<patch::mojom::FilePatcher>() {
  return sandbox::policy::SandboxType::kUtility;
}

// webapps::mojom::WebAppOriginAssociationParser
namespace webapps {
namespace mojom {
class WebAppOriginAssociationParser;
}
}  // namespace webapps
template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    webapps::mojom::WebAppOriginAssociationParser>() {
  return sandbox::policy::SandboxType::kUtility;
}

#endif  // COMPONENTS_SERVICES_SERVICE_SANDBOX_TYPE_H_
