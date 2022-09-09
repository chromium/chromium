// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_TEST_PATHS_H_
#define CHROME_SERVICES_CUPS_PROXY_TEST_PATHS_H_

namespace cups_proxy {

class Paths {
 public:
  enum {
    PATH_START = 1000,

    // Valid only in development and testing environments.
    DIR_TEST_DATA,
    PATH_END
  };

  // Call once to register the provider for the path keys defined above.
  static void RegisterPathProvider();
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_TEST_PATHS_H_
