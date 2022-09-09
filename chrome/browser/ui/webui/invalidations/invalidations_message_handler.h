// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_MESSAGE_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_logger_observer.h"
#include "components/invalidation/public/invalidation_util.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace invalidation {
class InvalidationLogger;
}  // namespace invalidation

// The implementation for the chrome://invalidations page.
class InvalidationsMessageHandler
    : public content::WebUIMessageHandler,
      public invalidation::InvalidationLoggerObserver {
 public:
  InvalidationsMessageHandler();

  InvalidationsMessageHandler(const InvalidationsMessageHandler&) = delete;
  InvalidationsMessageHandler& operator=(const InvalidationsMessageHandler&) =
      delete;

  ~InvalidationsMessageHandler() override;

  // Implementation of InvalidationLoggerObserver.
  void OnRegistrationChange(
      const std::set<std::string>& registered_handlers) override;
  void OnStateChange(const invalidation::InvalidatorState& new_state,
                     const base::Time& last_change_timestamp) override;
  void OnUpdatedTopics(
      const std::string& handler_name,
      const invalidation::TopicCountMap& topics_counts) override;
  void OnDebugMessage(const base::Value::Dict& details) override;
  void OnInvalidation(
      const invalidation::TopicInvalidationMap& new_invalidations) override;
  void OnDetailedStatus(base::Value::Dict network_details) override;

  // Implementation of WebUIMessageHandler.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // Triggers the logger to send the current state and objects ids.
  void UpdateContent(const base::Value::List& args);

  // Called by the javascript whenever the page is ready to receive messages.
  void UIReady(const base::Value::List& args);

  // Calls the InvalidationService for any internal details.
  void HandleRequestDetailedStatus(const base::Value::List& args);

 private:
  // The pointer to the internal InvalidatorService InvalidationLogger.
  // Used to get the information necessary to display to the JS and to
  // register ourselves as Observers for any notifications.
  raw_ptr<invalidation::InvalidationLogger> logger_;

  base::WeakPtrFactory<InvalidationsMessageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_MESSAGE_HANDLER_H_
