// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTED_APP_SIZE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTED_APP_SIZE_H_

#include <cstdint>
#include <optional>

#include "url/origin.h"

namespace web_app {

class ComputedAppSizeWithOrigin {
 public:
  ComputedAppSizeWithOrigin();
  ~ComputedAppSizeWithOrigin();

  ComputedAppSizeWithOrigin(ComputedAppSizeWithOrigin const&);
  // This CHECK-fails if `origin` is populated but not valid or empty
  ComputedAppSizeWithOrigin(uint64_t app_size_in_bytes,
                            uint64_t data_size_in_bytes,
                            std::optional<url::Origin> origin);

  uint64_t app_size_in_bytes() const;

  uint64_t data_size_in_bytes() const;

  const std::optional<url::Origin> origin() const;

 private:
  uint64_t app_size_in_bytes_ = 0;
  uint64_t data_size_in_bytes_ = 0;
  std::optional<url::Origin> origin_ = std::nullopt;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTED_APP_SIZE_H_
