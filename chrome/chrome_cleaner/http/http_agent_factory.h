// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_FACTORY_H_
#define CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_FACTORY_H_

#include <memory>

namespace chrome_cleaner {
class HttpAgent;
}  // namespace chrome_cleaner

namespace chrome_cleaner {

// Factory for creating HttpAgent objects. The default implementation will
// create chrome_cleaner::HttpAgentImpl objects. Used to allow tests to mock out
// an HttpAgent (see mock_http_agent_factory.{h,cc}).
class HttpAgentFactory {
 public:
  virtual ~HttpAgentFactory();

  // Returns an HttpAgent instance.
  virtual std::unique_ptr<chrome_cleaner::HttpAgent> CreateHttpAgent() const;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_FACTORY_H_
