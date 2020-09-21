// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_URL_LOADER_H_
#define CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_URL_LOADER_H_

#include <cstdint>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace chromeos {
namespace bloom {

// A wrapper class around |SimpleURLLoader| that allows us to provide fake data
// during unittests.
class BloomURLLoader {
 public:
  using Callback = base::OnceCallback<void(base::Optional<std::string> reply)>;

  virtual ~BloomURLLoader() = default;

  virtual void SendPostRequest(const GURL& url,
                               const std::string& access_token,
                               std::string&& body,
                               const std::string& mime_type,
                               Callback callback) = 0;
  virtual void SendGetRequest(const GURL& url,
                              const std::string& access_token,
                              Callback callback) = 0;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_SERVER_BLOOM_URL_LOADER_H_
