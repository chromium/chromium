// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_PDF_FONTCONFIG_MATCHING_H_
#define COMPONENTS_SERVICES_FONT_PDF_FONTCONFIG_MATCHING_H_

#include <stdint.h>
#include "build/build_config.h"
#include "pdf/buildflags.h"

#include <string>

#if !BUILDFLAG(ENABLE_PDF)
#error "Requires enable_pdf = true"
#endif

namespace font_service {

int MatchFontFaceWithFallback(const std::string& face,
                              bool is_bold,
                              bool is_italic,
                              uint32_t charset,
                              uint32_t fallback_family);

}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_PDF_FONTCONFIG_MATCHING_H_
