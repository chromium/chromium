// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/url_translator.h"

#include "build/branding_buildflags.h"
#include "net/base/url_util.h"

namespace syncer {

namespace {
// Parameters that the server understands. (here, a-Z)
const char kParameterClient[] = "client";
const char kParameterClientID[] = "client_id";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kClientName[] = "Google Chrome";
#else
const char kClientName[] = "Chromium";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace

GURL AppendSyncQueryString(const GURL& base, const std::string& client_id) {
  GURL result = net::AppendQueryParameter(base, kParameterClient, kClientName);
  result = net::AppendQueryParameter(result, kParameterClientID, client_id);
  return result;
}

}  // namespace syncer
