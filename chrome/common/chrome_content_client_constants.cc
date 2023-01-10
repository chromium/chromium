// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#include "chrome/common/chrome_content_client.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const base::FilePath::CharType ChromeContentClient::kNotPresent[] =
    FILE_PATH_LITERAL("internal-not-yet-present");
#endif

const base::FilePath::CharType ChromeContentClient::kPDFExtensionPluginPath[] =
    FILE_PATH_LITERAL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/");
const base::FilePath::CharType ChromeContentClient::kPDFInternalPluginPath[] =
    FILE_PATH_LITERAL("internal-pdf-viewer");
