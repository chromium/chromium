// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UTILS_H_

#include <openssl/base.h>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/net/server_certificate_database.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
// Enumeration of certificate management permissions which corresponds to
// values of policy ClientCertificateManagementAllowed.
// Underlying type is int because values are casting to/from prefs values.
enum class ClientCertificateManagementPermission : int {
  // Allow users to manage all certificates
  kAll = 0,
  // Allow users to manage user certificates
  kUserOnly = 1,
  // Disallow users from managing certificates
  kNone = 2
};
#endif  // BUILDFLAG(IS_CHROMEOS)

// Enumeration of certificate management permissions which corresponds to
// values of policy CACertificateManagementAllowed.
// Underlying type is int because values are casting to/from prefs values.
enum class CACertificateManagementPermission : int {
  // Allow users to manage all certificates
  kAll = 0,
  // Allow users to manage user certificates
  kUserOnly = 1,
  // Disallow users from managing certificates
  kNone = 2
};

void ShowCertificateDialog(base::WeakPtr<content::WebContents> web_contents,
                           bssl::UniquePtr<CRYPTO_BUFFER> cert);

void ShowCertificateDialog(
    base::WeakPtr<content::WebContents> web_contents,
    bssl::UniquePtr<CRYPTO_BUFFER> cert,
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata);

bool IsCACertificateManagementAllowed(const PrefService& prefs);

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UTILS_H_
