// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UTILS_H_

#include <openssl/base.h>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"

void ShowCertificateDialog(base::WeakPtr<content::WebContents> web_contents,
                           bssl::UniquePtr<CRYPTO_BUFFER> cert);
#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UTILS_H_
