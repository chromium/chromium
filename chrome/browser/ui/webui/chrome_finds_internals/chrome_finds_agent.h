// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_AGENT_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_AGENT_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace history {
class HistoryService;
}

namespace chrome_finds_internals {

// A profile-keyed agent for Chrome Finds that manages events and notifies
// observers.
class ChromeFindsAgent : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogMessageAdded(const std::string& message) = 0;
  };

  ChromeFindsAgent(OptimizationGuideKeyedService* opt_guide_service,
                   history::HistoryService* history_service);
  ~ChromeFindsAgent() override;

  ChromeFindsAgent(const ChromeFindsAgent&) = delete;
  ChromeFindsAgent& operator=(const ChromeFindsAgent&) = delete;

  // Adds a log message and notifies observers.
  void AddLogMessage(const std::string& message);

  // Starts the AI notification process.
  void Start(const std::string& prompt, int32_t history_count);

  // Retrieves history and returns it as a JSON string via callback.
  void GetHistoryJson(int32_t history_count,
                      base::OnceCallback<void(const std::string&)> callback);

  // Returns all stored log messages.
  const std::vector<std::string>& GetLogs() const { return logs_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnHistoryQueryComplete(history::QueryResults results);
  void OnModelExecutionComplete(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  raw_ptr<OptimizationGuideKeyedService> opt_guide_service_;
  raw_ptr<history::HistoryService> history_service_;
  std::vector<std::string> logs_;
  base::ObserverList<Observer> observers_;
  base::CancelableTaskTracker history_task_tracker_;
  std::string pending_prompt_;

  base::WeakPtrFactory<ChromeFindsAgent> weak_ptr_factory_{this};
};

}  // namespace chrome_finds_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_AGENT_H_
