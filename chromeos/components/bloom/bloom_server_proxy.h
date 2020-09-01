// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_SERVER_PROXY_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_SERVER_PROXY_H_

#include <string>
#include "base/callback.h"
#include "chromeos/components/bloom/screenshot_grabber.h"

namespace chromeos {
namespace bloom {

// Local object that handles all communication with the Bloom servers.
class BloomServerProxy {
 public:
  using Callback =
      base::OnceCallback<void(base::Optional<std::string> response)>;

  BloomServerProxy() = default;
  BloomServerProxy(BloomServerProxy&) = delete;
  BloomServerProxy& operator=(BloomServerProxy&) = delete;
  virtual ~BloomServerProxy() = default;

  // Send the screenshot to the Bloom server for analysis, and send the response
  // to |callback|.
  virtual void AnalyzeProblem(const std::string& access_token,
                              const Screenshot screenshot,
                              Callback callback) = 0;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_SERVER_PROXY_H_
