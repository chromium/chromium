// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager_util.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

// Before:
// Gesture(0) -> Gesture(1) -> NonGesture(2..11) -> Gesture(12)
// After:
// Gesture(0) -> Gesture(1) -> NonGesture(2..5) -> Empty(6..8) ->
// NonGesture(9..11) -> Gesture(12)
TEST(SBNavigationObserverManagerUtilTest, RemoveMiddleNonUserGestureEntries) {
  ReferrerChain referrer_chain;

  // Create a referrer chain with one landing page, one landing referrer, 10
  // client redirects, and lastly one more landing page.
  std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
      std::make_unique<ReferrerChainEntry>();
  referrer_chain_entry->set_navigation_initiation(
      ReferrerChainEntry::BROWSER_INITIATED);
  referrer_chain_entry->set_type(ReferrerChainEntry::LANDING_PAGE);
  referrer_chain_entry->set_url("http://landing_page.com");
  referrer_chain.Add()->Swap(referrer_chain_entry.get());

  referrer_chain_entry->set_navigation_initiation(
      ReferrerChainEntry::BROWSER_INITIATED);
  referrer_chain_entry->set_type(ReferrerChainEntry::LANDING_REFERRER);
  referrer_chain_entry->set_url("http://landing_referrer.com");
  referrer_chain.Add()->Swap(referrer_chain_entry.get());

  for (int i = 0; i < 10; i++) {
    std::unique_ptr<ReferrerChainEntry> client_redirect_entry =
        std::make_unique<ReferrerChainEntry>();
    client_redirect_entry->set_navigation_initiation(
        ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE);
    client_redirect_entry->set_type(ReferrerChainEntry::CLIENT_REDIRECT);
    client_redirect_entry->set_url("http://client_redirect.com" +
                                   base::NumberToString(i));
    referrer_chain.Add()->Swap(client_redirect_entry.get());
  }

  referrer_chain_entry->set_navigation_initiation(
      ReferrerChainEntry::BROWSER_INITIATED);
  referrer_chain_entry->set_type(ReferrerChainEntry::LANDING_REFERRER);
  referrer_chain_entry->set_url("http://landing_page.com");
  referrer_chain.Add()->Swap(referrer_chain_entry.get());

  std::vector<int> non_user_gesture_indices = {0, 1, 12};
  EXPECT_EQ(non_user_gesture_indices,
            GetUserGestureNavigationEntriesIndices(&referrer_chain));

  RemoveNonUserGestureReferrerEntries(&referrer_chain, 10);

  EXPECT_EQ("http://landing_page.com", referrer_chain[0].url());
  EXPECT_EQ("http://landing_referrer.com", referrer_chain[1].url());
  int index = 2;
  for (int counter = 0; counter < 10; counter++) {
    // The middle entries should have an empty ReferrerChainEntry.
    if (index == 6 || index == 7 || index == 8) {
      EXPECT_EQ("", referrer_chain[index].url());
    } else {
      EXPECT_EQ("http://client_redirect.com" + base::NumberToString(counter),
                referrer_chain[index].url());
    }
    index++;
  }
  EXPECT_EQ("http://landing_page.com", referrer_chain[12].url());
}

TEST(SBNavigationObserverManagerUtilTest,
     DoNotRemoveMiddleNonUserGestureEntriesIfLengthIsLessThanMaxAllowed) {
  ReferrerChain referrer_chain;
  for (int index = 0; index < 13; index++) {
    if (index == 0 || index == 1 || index == 12) {
      std::unique_ptr<ReferrerChainEntry> user_gesture_entry =
          std::make_unique<ReferrerChainEntry>();
      user_gesture_entry->set_navigation_initiation(
          ReferrerChainEntry::BROWSER_INITIATED);
      user_gesture_entry->set_type(ReferrerChainEntry::LANDING_REFERRER);
      user_gesture_entry->set_url("http://landing_referrer.com");
      referrer_chain.Add()->Swap(user_gesture_entry.get());
    } else {
      std::unique_ptr<ReferrerChainEntry> client_redirect_entry =
          std::make_unique<ReferrerChainEntry>();
      client_redirect_entry->set_navigation_initiation(
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE);
      client_redirect_entry->set_type(ReferrerChainEntry::CLIENT_REDIRECT);
      client_redirect_entry->set_url("http://client_redirect.com" +
                                     base::NumberToString(index));
      referrer_chain.Add()->Swap(client_redirect_entry.get());
    }
  }

  std::vector<int> non_user_gesture_indices = {0, 1, 12};
  EXPECT_EQ(non_user_gesture_indices,
            GetUserGestureNavigationEntriesIndices(&referrer_chain));

  RemoveNonUserGestureReferrerEntries(&referrer_chain, 13);
  // Nothing should be removed because the max length is 13.
  for (int index = 0; index < 13; index++) {
    if (index == 0 || index == 1 || index == 12) {
      EXPECT_EQ("http://landing_referrer.com", referrer_chain[index].url());
    } else {
      EXPECT_EQ("http://client_redirect.com" + base::NumberToString(index),
                referrer_chain[index].url());
    }
  }
}

// Before:
// Gesture(0) -> NonGesture(1...4) -> Gesture(5) -> NonGesture(6...9) ->
// Gesture(10) -> NonGesture(11...12) After: Gesture(0) -> NonGesture(1...2) ->
// Empty(3) -> NonGesture(4) -> Gesture(5) -> NonGesture(6)  -> Empty(7...8) ->
// NonGesture(8..9) -> Gesture(10) -> NonGesture(11...12)
TEST(SBNavigationObserverManagerUtilTest,
     RemoveNonConsecutiveMiddleNonUserGestureEntries) {
  ReferrerChain referrer_chain;

  // Create a referrer chain with entries alternating between 1 user
  // gesture entry and 4 non user gesture entries.
  for (int index = 0; index < 13; index++) {
    if (index == 0 || index == 5 || index == 10) {
      std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
          std::make_unique<ReferrerChainEntry>();
      referrer_chain_entry->set_navigation_initiation(
          ReferrerChainEntry::BROWSER_INITIATED);
      referrer_chain_entry->set_type(ReferrerChainEntry::LANDING_PAGE);
      referrer_chain_entry->set_url("http://landing_page.com");
      referrer_chain.Add()->Swap(referrer_chain_entry.get());
    } else {
      std::unique_ptr<ReferrerChainEntry> client_redirect_entry =
          std::make_unique<ReferrerChainEntry>();
      client_redirect_entry->set_navigation_initiation(
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE);
      client_redirect_entry->set_type(ReferrerChainEntry::CLIENT_REDIRECT);
      client_redirect_entry->set_url("http://client_redirect.com" +
                                     base::NumberToString(index));
      referrer_chain.Add()->Swap(client_redirect_entry.get());
    }
  }

  std::vector<int> non_user_gesture_indices = {0, 5, 10};
  EXPECT_EQ(non_user_gesture_indices,
            GetUserGestureNavigationEntriesIndices(&referrer_chain));

  RemoveNonUserGestureReferrerEntries(&referrer_chain, 10);

  for (int index = 0; index < 13; index++) {
    if (index == 0 || index == 5 || index == 10) {
      EXPECT_EQ("http://landing_page.com", referrer_chain[0].url());
    } else if (index == 3 || index == 7 || index == 8) {
      EXPECT_EQ("", referrer_chain[index].url());
    } else {
      EXPECT_EQ("http://client_redirect.com" + base::NumberToString(index),
                referrer_chain[index].url());
    }
  }
}

// Before:
// Gesture(0) -> NonGesture(1) -> Gesture(2) -> NG(3) -> G(4) -> NG(5) -> G(6)
// -> NG(7) -> G(8) After: Gesture(0) -> NonGesture(1) -> Gesture(2) -> E(3) ->
// G(4) -> E(5) -> G(6) -> E(7) -> G(8)
TEST(SBNavigationObserverManagerUtilTest,
     RemoveNonMiddleNonUserGestureEntries) {
  ReferrerChain referrer_chain;
  // Create a referrer chain with alternating user gesture and non user gesture
  // entries.
  for (int index = 0; index < 8; index++) {
    if ((index % 2) == 0) {
      std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
          std::make_unique<ReferrerChainEntry>();
      referrer_chain_entry->set_navigation_initiation(
          ReferrerChainEntry::BROWSER_INITIATED);
      referrer_chain_entry->set_type(ReferrerChainEntry::LANDING_PAGE);
      referrer_chain_entry->set_url("http://landing_page.com");
      referrer_chain.Add()->Swap(referrer_chain_entry.get());
    } else {
      std::unique_ptr<ReferrerChainEntry> client_redirect_entry =
          std::make_unique<ReferrerChainEntry>();
      client_redirect_entry->set_navigation_initiation(
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE);
      client_redirect_entry->set_type(ReferrerChainEntry::CLIENT_REDIRECT);
      client_redirect_entry->set_url("http://client_redirect.com");
      referrer_chain.Add()->Swap(client_redirect_entry.get());
    }
  }

  std::vector<int> non_user_gesture_indices = {0, 2, 4, 6};
  EXPECT_EQ(non_user_gesture_indices,
            GetUserGestureNavigationEntriesIndices(&referrer_chain));

  RemoveNonUserGestureReferrerEntries(&referrer_chain, 5);

  for (int index = 0; index < 8; index++) {
    if ((index % 2) == 0) {
      EXPECT_EQ("http://landing_page.com", referrer_chain[index].url());
    } else if (index == 3 || index == 5 || index == 7) {
      EXPECT_EQ("", referrer_chain[index].url());
    } else {
      EXPECT_EQ("http://client_redirect.com", referrer_chain[index].url());
    }
  }
}

// Before:
// NonGesture(0) -> Gesture(1) -> NonGesture(2) -> G(3) -> NG(4) -> G(5) ->
// NG(6) -> G(7) -> NG(8) After: NonGesture(0) -> Gesture(1) -> E(2) -> G(3) ->
// E(4) -> G(5) -> E(6) -> G(7) -> NG(8)
TEST(SBNavigationObserverManagerUtilTest,
     RemoveNonMiddleNonUserGestureEntriesWithClientRedirectStart) {
  ReferrerChain referrer_chain;
  // Create a referrer chain with alternating user gesture and non user gesture
  // entries.
  for (int index = 0; index < 8; index++) {
    if ((index % 2) == 0) {
      std::unique_ptr<ReferrerChainEntry> client_redirect_entry =
          std::make_unique<ReferrerChainEntry>();
      client_redirect_entry->set_navigation_initiation(
          ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE);
      client_redirect_entry->set_type(ReferrerChainEntry::CLIENT_REDIRECT);
      client_redirect_entry->set_url("http://client_redirect.com");
      referrer_chain.Add()->Swap(client_redirect_entry.get());
    } else {
      std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
          std::make_unique<ReferrerChainEntry>();
      referrer_chain_entry->set_navigation_initiation(
          ReferrerChainEntry::BROWSER_INITIATED);
      referrer_chain_entry->set_type(ReferrerChainEntry::LANDING_PAGE);
      referrer_chain_entry->set_url("http://landing_page.com");
      referrer_chain.Add()->Swap(referrer_chain_entry.get());
    }
  }

  std::vector<int> non_user_gesture_indices = {1, 3, 5, 7};
  EXPECT_EQ(non_user_gesture_indices,
            GetUserGestureNavigationEntriesIndices(&referrer_chain));

  RemoveNonUserGestureReferrerEntries(&referrer_chain, 5);

  for (int index = 0; index < 8; index++) {
    if (index == 2 || index == 4 || index == 6) {
      EXPECT_EQ("", referrer_chain[index].url());
    } else if ((index % 2) == 0) {
      EXPECT_EQ("http://client_redirect.com", referrer_chain[index].url());
    } else {
      EXPECT_EQ("http://landing_page.com", referrer_chain[index].url());
    }
  }
}
}  // namespace safe_browsing
