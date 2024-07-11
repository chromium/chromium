// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CLIENT_CERT_SOURCES_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CLIENT_CERT_SOURCES_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"

class Profile;

std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreatePlatformClientCertSource();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreateProvisionedClientCertSource(Profile* profile);
#endif

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreateExtensionsClientCertSource(Profile* profile);
#endif

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CLIENT_CERT_SOURCES_H_
