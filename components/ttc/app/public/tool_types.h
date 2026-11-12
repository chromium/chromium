// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_TOOL_TYPES_H_
#define COMPONENTS_TTC_APP_PUBLIC_TOOL_TYPES_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"

namespace ttc {

// Defines a tool registered with the model execution backend.
struct ToolDefinition {
  // Behavior specifies whether model execution should block waiting for tool
  // results or continue asynchronously.
  enum class Behavior {
    // Model execution blocks until tool execution completes and returns
    // results.
    kBlocking,
    // Model execution continues without blocking on tool execution results.
    kNonBlocking,
  };

  // Verbalization specifies whether the model should speak or describe its
  // action to the user while/after executing the tool.
  enum class Verbalization {
    // Standard verbalization where the model can speak or acknowledge tool
    // actions.
    kStandard,
    // Silent action where the tool executes without spoken verbalization from
    // the model.
    kSilentAction,
  };

  ToolDefinition();
  ~ToolDefinition();
  ToolDefinition(ToolDefinition&&);
  ToolDefinition& operator=(ToolDefinition&&);
  ToolDefinition(const ToolDefinition&) = delete;
  ToolDefinition& operator=(const ToolDefinition&) = delete;

  ToolDefinition Clone() const;

  std::string name;
  std::string description;
  base::DictValue parameters_json_schema;
  Behavior behavior = Behavior::kBlocking;
  Verbalization verbalization = Verbalization::kStandard;
};

// Parameters for a tool call.
struct ToolRequest {
  ToolRequest Clone() const;

  std::string name;
  base::DictValue arguments;
  // The server's identifier for this call. Used to correlate debug records.
  std::string call_id;
};

// Details of a failed tool call. At least one of the fields is always set.
struct ToolError {
  // The actor error code, if the failure came from an actor tool. Never kOk.
  std::optional<actor::mojom::ActionResultCode> code;

  // An English language description of the failure. If absent, `code` is set
  // and callers should describe the failure using it.
  std::optional<std::string> message;
};

// An image produced by a tool for the model to see, e.g. a screenshot.
struct VisualContent {
  // Encoded image bytes.
  std::vector<uint8_t> image_data;
  // MIME type of `image_data`, e.g. "image/jpeg".
  std::string mime_type;
};

// The result of executing a tool: either a success, which may carry a
// tool-specific result dict and an image, or a failure, which carries an actor
// error code and/or a message. Instances can only be created through the
// factories below.
class ToolResponse {
 public:
  // Returns a response for a tool call that succeeded. `result` holds
  // tool-specific values to report back to the model, and may be empty.
  static ToolResponse Success(base::DictValue result = base::DictValue());

  // As above, with an image for the model. `visual_content` must have
  // non-empty data and MIME type.
  static ToolResponse Success(base::DictValue result,
                              VisualContent visual_content);

  // Returns a response for a tool call that failed with actor error `code`,
  // which must not be kOk. An empty `message` is treated as absent, leaving
  // the failure described by `code` alone.
  static ToolResponse Error(actor::mojom::ActionResultCode code,
                            std::string message = std::string());

  // Returns a response for a tool call that failed for a reason that has no
  // corresponding actor error code. `message` must not be empty.
  static ToolResponse Error(std::string message);

  ToolResponse(ToolResponse&&) noexcept;
  ToolResponse& operator=(ToolResponse&&) noexcept;
  ~ToolResponse();

  ToolResponse(const ToolResponse&) = delete;
  ToolResponse& operator=(const ToolResponse&) = delete;

  // Whether the tool call succeeded.
  bool Ok() const;

  // The tool-specific result values, which may be empty. Valid only if Ok().
  const base::DictValue& GetResult() const;
  base::DictValue TakeResult() &&;

  // The image for the model, if any. Always absent if !Ok().
  const std::optional<VisualContent>& visual_content() const {
    return visual_content_;
  }

  // Details of the failure. Valid only if !Ok().
  const ToolError& error() const;

 private:
  explicit ToolResponse(
      std::variant<base::DictValue, ToolError> value,
      std::optional<VisualContent> visual_content = std::nullopt);

  std::variant<base::DictValue, ToolError> value_;
  std::optional<VisualContent> visual_content_;
};

using ToolResponseCallback = base::OnceCallback<void(ToolResponse)>;

// A tool call to run, with the callback that receives its result.
struct PendingToolCall {
  PendingToolCall(ToolRequest request, ToolResponseCallback callback);
  PendingToolCall(PendingToolCall&&) noexcept;
  PendingToolCall& operator=(PendingToolCall&&) noexcept;
  ~PendingToolCall();

  ToolRequest request;
  ToolResponseCallback callback;
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_TOOL_TYPES_H_
