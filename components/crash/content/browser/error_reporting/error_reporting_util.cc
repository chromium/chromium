// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/error_reporting/error_reporting_util.h"

#include "url/gurl.h"

std::string RedactUrlForErrorReports(const GURL& url) {
  if (!url.is_valid()) {
    return std::string();
  }

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}
