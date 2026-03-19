// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_info.h"

#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockObserver : public base::CheckedObserver {
 public:
  void OnMessage(std::string message) {
    received_messages.push_back(std::move(message));
  }
  std::vector<std::string> received_messages;
};

class TestDownloadBubbleInfo : public DownloadBubbleInfo<MockObserver> {
 public:
  using DownloadBubbleInfo<MockObserver>::NotifyObservers;
};

// Regression test to ensure that NotifyObservers forwards parameters correctly.
TEST(DownloadBubbleInfoTest, NotifyObserversMovesRvalue) {
  TestDownloadBubbleInfo info;
  MockObserver obs1;
  MockObserver obs2;
  info.AddObserver(&obs1);
  info.AddObserver(&obs2);

  std::string msg = "hello";
  info.NotifyObservers(&MockObserver::OnMessage, std::move(msg));

  ASSERT_EQ(obs1.received_messages.size(), 1u);
  EXPECT_EQ(obs1.received_messages[0], "hello");

  ASSERT_EQ(obs2.received_messages.size(), 1u);
  EXPECT_EQ(obs2.received_messages[0], "hello");
}

}  // namespace
