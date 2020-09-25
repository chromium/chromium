// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/test/test_helper.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "components/media_router/common/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

MockIssuesObserver::MockIssuesObserver(IssueManager* issue_manager)
    : IssuesObserver(issue_manager) {}
MockIssuesObserver::~MockIssuesObserver() = default;

MockMediaSinksObserver::MockMediaSinksObserver(MediaRouter* router,
                                               const MediaSource& source,
                                               const url::Origin& origin)
    : MediaSinksObserver(router, source, origin) {}
MockMediaSinksObserver::~MockMediaSinksObserver() = default;

MockMediaRoutesObserver::MockMediaRoutesObserver(
    MediaRouter* router,
    const MediaSource::Id source_id)
    : MediaRoutesObserver(router, source_id) {}
MockMediaRoutesObserver::~MockMediaRoutesObserver() = default;

MockPresentationConnectionProxy::MockPresentationConnectionProxy() = default;
MockPresentationConnectionProxy::~MockPresentationConnectionProxy() = default;

}  // namespace media_router
