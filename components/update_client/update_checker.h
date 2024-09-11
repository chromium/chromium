// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/component.h"
#include "components/update_client/protocol_parser.h"
#include "url/gurl.h"

namespace update_client {

class Configurator;
struct UpdateContext;

class UpdateChecker {
 public:
  using UpdateCheckCallback = base::OnceCallback<void(
      const std::optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec)>;

  using Factory = base::RepeatingCallback<std::unique_ptr<UpdateChecker>(
      scoped_refptr<Configurator> config)>;

  UpdateChecker(const UpdateChecker&) = delete;
  UpdateChecker& operator=(const UpdateChecker&) = delete;

  virtual ~UpdateChecker() = default;

  // Initiates an update check for the components specified by their ids.
  // `update_context` contains the updateable apps. `additional_attributes`
  // specifies any extra request data to send. On completion, the state of
  // `components` is mutated as required by the server response received.
  virtual void CheckForUpdates(
      scoped_refptr<UpdateContext> update_context,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) = 0;

  static std::unique_ptr<UpdateChecker> Create(
      scoped_refptr<Configurator> config);

 protected:
  UpdateChecker() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
