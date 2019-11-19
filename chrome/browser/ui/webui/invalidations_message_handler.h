// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_MESSAGE_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~InvalidationsMessageHandler() override;

  // Implementation of InvalidationLoggerObserver.
  void OnRegistrationChange(
      const std::multiset<std::string>& registered_handlers) override;
  void OnStateChange(const syncer::InvalidatorState& new_state,
                     const base::Time& last_change_timestamp) override;
  void OnUpdateIds(const std::string& handler_name,
                   const syncer::ObjectIdCountMap& ids_set) override;
  void OnDebugMessage(const base::DictionaryValue& details) override;
  void OnInvalidation(
      const syncer::ObjectIdInvalidationMap& new_invalidations) override;
  void OnDetailedStatus(const base::DictionaryValue& network_details) override;

  // Implementation of WebUIMessageHandler.
  void RegisterMessages() override;

  // Triggers the logger to send the current state and objects ids.
  void UpdateContent(const base::ListValue* args);

  // Called by the javascript whenever the page is ready to receive messages.
  void UIReady(const base::ListValue* args);

  // Calls the InvalidationService for any internal details.
  void HandleRequestDetailedStatus(const base::ListValue* args);

 private:
  // The pointer to the internal InvalidatorService InvalidationLogger.
  // Used to get the information necessary to display to the JS and to
  // register ourselves as Observers for any notifications.
  invalidation::InvalidationLogger* logger_;

  base::WeakPtrFactory<InvalidationsMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InvalidationsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_MESSAGE_HANDLER_H_
