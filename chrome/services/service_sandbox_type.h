// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SERVICE_SANDBOX_TYPE_H_
#define CHROME_SERVICES_SERVICE_SANDBOX_TYPE_H_

#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/sandbox_type.h"

// This file maps service classes to sandbox types. See
// ServiceProcessHost::Launch() for how these templates are consumed.

// chrome::mojom::MediaParserFactory
namespace chrome {
namespace mojom {
class MediaParserFactory;
}
}  // namespace chrome
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<chrome::mojom::MediaParserFactory>() {
  return sandbox::policy::SandboxType::kUtility;
}

// ipp_parser::mojom::IppParser
namespace ipp_parser {
namespace mojom {
class IppParser;
}
}  // namespace ipp_parser
template <>
inline sandbox::policy::SandboxType
content::GetServiceSandboxType<ipp_parser::mojom::IppParser>() {
  return sandbox::policy::SandboxType::kUtility;
}

// qrcode_generator::mojom::QRCodeGeneratorService
namespace qrcode_generator {
namespace mojom {
class QRCodeGeneratorService;
}
}  // namespace qrcode_generator
template <>
inline sandbox::policy::SandboxType content::GetServiceSandboxType<
    qrcode_generator::mojom::QRCodeGeneratorService>() {
  return sandbox::policy::SandboxType::kUtility;
}

#endif  // CHROME_SERVICES_SERVICE_SANDBOX_TYPE_H_
