// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_NET_UTIL_H_
#define CHROME_TEST_CHROMEDRIVER_NET_NET_UTIL_H_

#include <string>

#include "base/memory/scoped_refptr.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace network::mojom {
class URLLoaderFactory;
}  // namespace network::mojom

class NetAddress {
 public:
  NetAddress();  // Creates an invalid address.
  explicit NetAddress(int port);  // Host is set to localhost.
  NetAddress(const std::string& host, int port);
  ~NetAddress();

  bool IsValid() const;

  // Returns host:port.
  std::string ToString() const;

  const std::string& host() const;
  int port() const;

 private:
  std::string host_;
  int port_;
};

void SetIOCapableTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner);

// Synchronously fetches data from a GET HTTP request to the given URL.
// Returns true if response is 200 OK and sets response body to |response|.
bool FetchUrl(const std::string& url,
              network::mojom::URLLoaderFactory* factory,
              std::string* response);

// Synchronously fetches data from a GET HTTP request to the given URL.
// Returns true if response is 200 OK and sets response body to |response|.
bool FetchUrl(const GURL& url,
              network::mojom::URLLoaderFactory* factory,
              std::string* response);

#endif  // CHROME_TEST_CHROMEDRIVER_NET_NET_UTIL_H_
