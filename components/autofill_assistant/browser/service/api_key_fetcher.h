// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_API_KEY_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_API_KEY_FETCHER_H_

#include <string>
#include "components/version_info/version_info.h"

namespace autofill_assistant {

// Wrapper interface to allow mocking this in unit tests.
struct ApiKeyFetcher {
  virtual std::string GetAPIKey(version_info::Channel channel);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_API_KEY_FETCHER_H_
