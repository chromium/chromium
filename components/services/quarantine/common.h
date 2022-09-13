// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_COMMON_H_
#define COMPONENTS_SERVICES_QUARANTINE_COMMON_H_

#include "url/gurl.h"

namespace quarantine {

// Return a sanitized URL to use as a source or referrer for the purpose of
// annotating the source of a downloaded file.
//
// For privacy reasons, it's best to not URLs as-is when annotating or
// informing third parties about the source of a download. Hence this function
// is used to strip out any unnecessary or excessive parts from the URL.
//
// See https://crbug.com/917842 for justification for the types of sanitization
// performed here. In particular, stripping fragments, query, and path from URLs
// was discussed and rejected due to that information being pertinent to
// evaluating whether the downloaded file is safe.
GURL SanitizeUrlForQuarantine(const GURL& source_url);

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_COMMON_H_
