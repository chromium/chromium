// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_DEDUPLICATION_URL_STRIP_HANDLER_H_
#define COMPONENTS_URL_DEDUPLICATION_URL_STRIP_HANDLER_H_

class GURL;

// Class to handle various methods of stripping URLs.
class URLStripHandler {
 public:
  // Strips params that are not useful for disambiguating urls.
  GURL StripExtraParams(GURL);
};

#endif  // COMPONENTS_URL_DEDUPLICATION_URL_STRIP_HANDLER_H_
