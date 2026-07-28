// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INFOBARS_BROWSER_INFOBAR_REGISTRY_H_
#define CHROME_BROWSER_UI_INFOBARS_BROWSER_INFOBAR_REGISTRY_H_

#include "build/branding_buildflags.h"
#include "build/buildflag.h"

namespace infobars {

// Registers all infobars supported by the centralized infobar framework.
void RegisterInfoBars();

#if BUILDFLAG(CHROME_FOR_TESTING)
void RegisterChromeForTestingInfoBar();
#endif

}  // namespace infobars

#endif  // CHROME_BROWSER_UI_INFOBARS_BROWSER_INFOBAR_REGISTRY_H_
