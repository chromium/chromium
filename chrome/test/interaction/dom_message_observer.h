// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_DOM_MESSAGE_OBSERVER_H_
#define CHROME_TEST_INTERACTION_DOM_MESSAGE_OBSERVER_H_

#include <string>
#include <vector>

#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"

// Base class that uses a DOMMessageQueue to observe messages coming from an
// instrumented web page. The page must already be instrumented.
class DomMessageObserverBase {
 public:
  // This will hard-fail if `instrumented_webcontents` isn't actually an
  // instrumented WebContents.
  explicit DomMessageObserverBase(
      ui::ElementIdentifier instrumented_webcontents);

 protected:
  virtual void OnMessageReceived(const std::string& message) = 0;

 private:
  void AddCallback();
  void OnMessageAvailable();

  content::DOMMessageQueue queue_;
};

// Observes DOM messages emitted by `instrumented_webcontents`; the reported
// state is the most recent message.
class LatestDomMessageObserver : DomMessageObserverBase,
                                 public ui::test::StateObserver<std::string> {
 public:
  explicit LatestDomMessageObserver(
      ui::ElementIdentifier instrumented_webcontents);
  ~LatestDomMessageObserver() override;

 protected:
  // DomMessageObserverBase:
  void OnMessageReceived(const std::string& message) override;
};

// Observes DOM messages emitted by `instrumented_webcontents`; the reported
// state is the ordered list of all messages received.
class DomMessageHistoryObserver
    : DomMessageObserverBase,
      ui::test::StateObserver<std::vector<std::string>> {
 public:
  explicit DomMessageHistoryObserver(
      ui::ElementIdentifier instrumented_webcontents);
  ~DomMessageHistoryObserver() override;

 protected:
  // DomMessageObserverBase:
  void OnMessageReceived(const std::string& message) override;

 private:
  std::vector<std::string> messages_;
};

#endif  // CHROME_TEST_INTERACTION_DOM_MESSAGE_OBSERVER_H_
