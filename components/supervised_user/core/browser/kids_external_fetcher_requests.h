// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_REQUESTS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_REQUESTS_H_

#include "base/strings/strcat.h"
#include "components/supervised_user/core/browser/kids_external_fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "url/gurl.h"

namespace supervised_user {

// Determines the response type. See go/system-parameters to see list of
// possible One Platform system params.
constexpr base::StringPiece kSystemParameters("alt=proto");

// Creates a requests for kids management api which is independent from the
// current profile (doesn't take Profile* parameter). It also adds query
// parameter that configures the remote endpoint to respond with a protocol
// buffer message.
template <typename RequestType>
GURL CreateRequestUrl(const FetcherConfig& config) {
  return GURL(config.service_endpoint)
      .Resolve(base::StrCat({config.service_path, "?", kSystemParameters}));
}

}  // namespace supervised_user

#endif
