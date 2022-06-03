// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#include "chrome/common/chrome_content_client.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const base::FilePath::CharType ChromeContentClient::kNotPresent[] =
    FILE_PATH_LITERAL("internal-not-yet-present");
#endif

#if BUILDFLAG(ENABLE_NACL)
const base::FilePath::CharType ChromeContentClient::kNaClPluginFileName[] =
    FILE_PATH_LITERAL("internal-nacl-plugin");
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char ChromeContentClient::kPDFExtensionPluginName[] = "Chrome PDF Viewer";
const char ChromeContentClient::kPDFInternalPluginName[] = "Chrome PDF Plugin";
#else
const char ChromeContentClient::kPDFExtensionPluginName[] =
    "Chromium PDF Viewer";
const char ChromeContentClient::kPDFInternalPluginName[] =
    "Chromium PDF Plugin";
#endif

const base::FilePath::CharType ChromeContentClient::kPDFPluginPath[] =
    FILE_PATH_LITERAL("internal-pdf-viewer");
