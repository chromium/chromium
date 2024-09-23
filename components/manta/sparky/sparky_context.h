// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SPARKY_SPARKY_CONTEXT_H_
#define COMPONENTS_MANTA_SPARKY_SPARKY_CONTEXT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/sparky_util.h"

namespace manta {

// Stores the input Sparky data to be included into the Sparky Provider.
struct COMPONENT_EXPORT(MANTA) SparkyContext {
  explicit SparkyContext(const proto::Turn& latest_turn);

  SparkyContext(const proto::Turn& latest_turn,
                const std::string& page_content);

  ~SparkyContext();

  SparkyContext(const SparkyContext&);
  SparkyContext& operator=(const SparkyContext&);

  proto::Turn latest_turn;
  std::optional<DiagnosticsData> diagnostics_data;
  std::optional<std::string> page_content;
  std::optional<std::string> page_url;
  std::optional<std::string> server_url;
  std::vector<FileData> files;
  bool collect_settings{true};
  proto::Task task{proto::Task::TASK_PLANNER};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SPARKY_SPARKY_CONTEXT_H_
