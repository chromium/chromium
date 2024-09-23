// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PDF_UTIL_H_
#define CHROME_COMMON_PDF_UTIL_H_

#include <string>

class GURL;

// Returns the HTML contents of the placeholder.
std::string GetPDFPlaceholderHTML(const GURL& pdf_url);

#endif  // CHROME_COMMON_PDF_UTIL_H_
