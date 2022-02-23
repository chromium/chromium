// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/component.h"
#include "components/update_client/protocol_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace update_client {

class Configurator;
class PersistedData;

class UpdateChecker {
 public:
  using UpdateCheckCallback = base::OnceCallback<void(
      const absl::optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec)>;

  using Factory =
      std::unique_ptr<UpdateChecker> (*)(scoped_refptr<Configurator> config,
                                         PersistedData* persistent);

  UpdateChecker(const UpdateChecker&) = delete;
  UpdateChecker& operator=(const UpdateChecker&) = delete;

  virtual ~UpdateChecker() = default;

  // Initiates an update check for the components specified by their ids.
  // |additional_attributes| provides a way to customize the <request> element.
  // |is_foreground| controls the value of "X-Goog-Update-Interactivity"
  // header which is sent with the update check.
  // On completion, the state of |components| is mutated as required by the
  // server response received.
  virtual void CheckForUpdates(
      const std::string& session_id,
      const std::vector<std::string>& ids_to_check,
      const IdToComponentPtrMap& components,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) = 0;

  static std::unique_ptr<UpdateChecker> Create(
      scoped_refptr<Configurator> config,
      PersistedData* persistent);

 protected:
  UpdateChecker() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
