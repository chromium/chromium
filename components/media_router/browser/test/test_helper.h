// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_TEST_TEST_HELPER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_TEST_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/test/values_test_util.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

// Matcher for IssueInfo title.
MATCHER_P(IssueTitleEquals, title, "") {
  return arg.info().title == title;
}

MATCHER_P(StateChangeInfoEquals, other, "") {
  return arg.state == other.state && arg.close_reason == other.close_reason &&
         arg.message == other.message;
}

class MockIssuesObserver : public IssuesObserver {
 public:
  explicit MockIssuesObserver(IssueManager* issue_manager);
  ~MockIssuesObserver() override;

  MOCK_METHOD1(OnIssue, void(const Issue& issue));
  MOCK_METHOD0(OnIssuesCleared, void());
};

class MockMediaSinksObserver : public MediaSinksObserver {
 public:
  MockMediaSinksObserver(MediaRouter* router,
                         const MediaSource& source,
                         const url::Origin& origin);
  ~MockMediaSinksObserver() override;

  MOCK_METHOD1(OnSinksReceived, void(const std::vector<MediaSink>& sinks));
};

class MockMediaRoutesObserver : public MediaRoutesObserver {
 public:
  explicit MockMediaRoutesObserver(
      MediaRouter* router,
      const MediaSource::Id source_id = std::string());
  ~MockMediaRoutesObserver() override;

  MOCK_METHOD2(OnRoutesUpdated,
               void(const std::vector<MediaRoute>& routes,
                    const std::vector<MediaRoute::Id>& joinable_route_ids));
};

class MockPresentationConnectionProxy
    : public blink::mojom::PresentationConnection {
 public:
  MockPresentationConnectionProxy();
  ~MockPresentationConnectionProxy() override;
  MOCK_METHOD1(OnMessage, void(blink::mojom::PresentationConnectionMessagePtr));
  MOCK_METHOD1(DidChangeState,
               void(blink::mojom::PresentationConnectionState state));
  MOCK_METHOD1(DidClose, void(blink::mojom::PresentationConnectionCloseReason));
};

// Matcher for PresentationConnectionMessagePtr arguments.
MATCHER_P(IsPresentationConnectionMessage, json, "") {
  return arg->is_message() && base::test::IsJsonMatcher(json).MatchAndExplain(
                                  arg->get_message(), result_listener);
}

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_TEST_TEST_HELPER_H_
