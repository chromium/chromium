// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHILD_PDF_CHILD_INIT_H_
#define CHROME_CHILD_PDF_CHILD_INIT_H_

#include "pdf/buildflags.h"

#if !BUILDFLAG(ENABLE_PDF)
#error "PDF must be enabled"
#endif

// Possibly patches GDI's `GetFontData()` for use by PDFium.
void MaybePatchGdiGetFontData();

#endif  // CHROME_CHILD_PDF_CHILD_INIT_H_
