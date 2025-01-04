// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace tab_groups {

using collaboration::messaging::MessagingBackendService;
using collaboration::messaging::PersistentMessage;

// Dictates the intended display status of the message.
enum class MessageDisplayStatus {
  // The message should be displayed.
  kDisplay,

  // The message should be hidden.
  kHide,

  kMaxValue = kHide,
};

// This class is responsible for listener for and delivering messages from
// the MessagingBackendService for Desktop. Once the backend service has
// been initialized, this observer will query for any active (but
// previously undelivered) messages and populate the UI with various
// activity indicators.
class CollaborationMessagingObserver
    : public MessagingBackendService::PersistentMessageObserver,
      public KeyedService {
 public:
  explicit CollaborationMessagingObserver(Profile* profile);
  CollaborationMessagingObserver(const CollaborationMessagingObserver&) =
      delete;
  CollaborationMessagingObserver& operator=(
      const CollaborationMessagingObserver&) = delete;
  ~CollaborationMessagingObserver() override;

  // Testing-only method. Dispatch method as though it came from the
  // backend service.
  void DispatchMessageForTests(PersistentMessage message, bool display);

 protected:
  FRIEND_TEST_ALL_PREFIXES(CollaborationMessagingObserverBrowserTest,
                           HandlesMessages);
  FRIEND_TEST_ALL_PREFIXES(CollaborationMessagingObserverBrowserTest,
                           HandlesTabMessagesInCollapsedGroup);

  // MessagingBackendService::PersistentMessageObserver
  void OnMessagingBackendServiceInitialized() override;
  void DisplayPersistentMessage(PersistentMessage message) override;
  void HidePersistentMessage(PersistentMessage message) override;

 private:
  // Finds the tab group designated by this message and sets/hides an
  // attention indicator on the tab group header.
  void HandleDirtyTabGroup(PersistentMessage message,
                           MessageDisplayStatus display);

  // Finds the tab designated by this message and sets/hides an attention
  // indicator on the tab's icon.
  void HandleDirtyTab(PersistentMessage message, MessageDisplayStatus display);

  // Finds the tab designated by this message and sets/clears data in
  // TabFeatures to be used by the tab's Hovercard and PageAction.
  void HandleChip(PersistentMessage message, MessageDisplayStatus display);

  // All messages this observer receives are routed through this method
  // and dispatched to a handler. Treatment of the specific message is
  // delegated to the appropriate handler for this type.
  void DispatchMessage(PersistentMessage message, MessageDisplayStatus display);

  raw_ptr<Profile> profile_;

  base::ScopedObservation<MessagingBackendService,
                          MessagingBackendService::PersistentMessageObserver>
      messaging_service_observation_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_COLLABORATION_MESSAGING_OBSERVER_H_
