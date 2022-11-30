// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_COMMAND_LINE_TOP_HOST_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_COMMAND_LINE_TOP_HOST_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/optimization_guide/core/top_host_provider.h"

namespace optimization_guide {

// A TopHostProvider implementation that provides top hosts based on what is fed
// through the command line. This implementation is intended to be used just for
// developer and integration testing.
class CommandLineTopHostProvider : public TopHostProvider {
 public:
  // Creates a TopHostProvider if the flag for overriding top hosts has been
  // enabled.
  static std::unique_ptr<CommandLineTopHostProvider> CreateIfEnabled();

  CommandLineTopHostProvider(const CommandLineTopHostProvider&) = delete;
  CommandLineTopHostProvider& operator=(const CommandLineTopHostProvider&) =
      delete;

  ~CommandLineTopHostProvider() override;

  // TopHostProvider implementation:
  std::vector<std::string> GetTopHosts() override;

 private:
  explicit CommandLineTopHostProvider(
      const std::vector<std::string>& top_hosts);

  std::vector<std::string> top_hosts_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_COMMAND_LINE_TOP_HOST_PROVIDER_H_
