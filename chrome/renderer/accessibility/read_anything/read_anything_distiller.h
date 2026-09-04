// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_DISTILLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_DISTILLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {
class AXSerializableTree;
}  // namespace ui

// Represents the output of a distillation engine.
struct DistillationResult {
  enum class Type {
    kAXNodeIds,  // Screen2x
    kHTML,       // Readability
  };

  Type type = Type::kAXNodeIds;
  ui::AXTreeID tree_id;
  std::string title;
  std::vector<ui::AXNodeID> node_ids;
  std::string html_content;
};

// Encapsulates the input parameters needed for distillation.
struct DistillationRequest {
  raw_ptr<ui::AXSerializableTree> tree = nullptr;
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
};

// Abstract interface decoupling distillation engines from presentation.
class ReadAnythingDistiller {
 public:
  using DistillationCompleteCallback =
      base::RepeatingCallback<void(const DistillationResult& result)>;

  virtual ~ReadAnythingDistiller() = default;

  // Initiates distillation.
  virtual void Distill(const DistillationRequest& request) = 0;

  // Resets internal state.
  virtual void Reset() = 0;

  // Returns whether a distillation is currently in progress.
  virtual bool IsInProgress() const = 0;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_DISTILLER_H_
