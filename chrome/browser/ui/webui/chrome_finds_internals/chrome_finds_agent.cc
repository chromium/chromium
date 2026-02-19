// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_agent.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/values.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/string_value.pb.h"

namespace {

constexpr int kMaxHistoryEntries = 500;

}  // namespace

namespace chrome_finds_internals {

ChromeFindsAgent::ChromeFindsAgent(
    OptimizationGuideKeyedService* opt_guide_service,
    history::HistoryService* history_service)
    : opt_guide_service_(opt_guide_service),
      history_service_(history_service) {}

ChromeFindsAgent::~ChromeFindsAgent() = default;

void ChromeFindsAgent::AddLogMessage(const std::string& message) {
  logs_.push_back(message);
  for (auto& observer : observers_) {
    observer.OnLogMessageAdded(message);
  }
}

void ChromeFindsAgent::Start(const std::string& prompt, int32_t history_count) {
  AddLogMessage("ChromeFindsAgent::Start() called.");
  pending_prompt_ = prompt;

  if (history_count <= 0) {
    AddLogMessage("History count is 0 or less. Proceeding with empty history.");
    OnHistoryQueryComplete(history::QueryResults());
    return;
  }

  if (!history_service_) {
    AddLogMessage("Error: HistoryService not available.");
    return;
  }

  history::QueryOptions options;
  options.max_count = std::min(history_count, kMaxHistoryEntries);
  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&ChromeFindsAgent::OnHistoryQueryComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      &history_task_tracker_);
}

void ChromeFindsAgent::GetHistoryJson(
    int32_t history_count,
    base::OnceCallback<void(const std::string&)> callback) {
  if (history_count <= 0) {
    std::move(callback).Run("[]");
    return;
  }

  if (!history_service_) {
    std::move(callback).Run("{}");
    return;
  }

  history::QueryOptions options;
  options.max_count = std::min(history_count, kMaxHistoryEntries);
  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(
          [](base::OnceCallback<void(const std::string&)> callback,
             history::QueryResults results) {
            base::ListValue history_list;
            for (const auto& result : results) {
              base::DictValue entry;
              entry.Set("title", result.title());
              entry.Set("url", result.url().spec());
              entry.Set(
                  "visit_time",
                  static_cast<double>(
                      result.visit_time().InMillisecondsSinceUnixEpoch()));
              history_list.Append(std::move(entry));
            }
            std::string json;
            base::JSONWriter::WriteWithOptions(
                history_list,
                base::JSONWriter::OPTIONS_PRETTY_PRINT |
                    base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
                &json);
            std::move(callback).Run(json);
          },
          std::move(callback)),
      &history_task_tracker_);
}

void ChromeFindsAgent::OnHistoryQueryComplete(history::QueryResults results) {
  AddLogMessage(base::StringPrintf("History query complete. Found %zu results.",
                                   results.size()));

  std::string history_text;
  for (const auto& result : results) {
    history_text += base::StringPrintf(
        "- %s (%s)\n", base::UTF16ToUTF8(result.title()).c_str(),
        result.url().spec().c_str());
  }

  std::string prompt = pending_prompt_;
  size_t pos = prompt.find("{USER_HISTORY}");
  if (pos != std::string::npos) {
    prompt.replace(pos, 14, history_text);
  }

  AddLogMessage("Executing model with prompt...\n" + prompt);

  if (!opt_guide_service_) {
    AddLogMessage("Error: OptimizationGuideKeyedService not available.");
    return;
  }

  optimization_guide::proto::StringValue request;
  request.set_value(prompt);

  opt_guide_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kTest, request, {},
      base::BindOnce(&ChromeFindsAgent::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeFindsAgent::OnModelExecutionComplete(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.response.has_value()) {
    AddLogMessage("Model execution failed.");
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response.value());
  if (response) {
    AddLogMessage("Model execution successful. Response:");
    AddLogMessage(response->value());
  } else {
    AddLogMessage("Model execution successful, but failed to parse response.");
  }
}

void ChromeFindsAgent::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeFindsAgent::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chrome_finds_internals
