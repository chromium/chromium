// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/test/test_helper.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "components/media_router/common/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {
#if !BUILDFLAG(IS_ANDROID)
MockIssuesObserver::MockIssuesObserver(IssueManager* issue_manager)
    : IssuesObserver(issue_manager) {}
MockIssuesObserver::~MockIssuesObserver() = default;
#endif  // !BUILDFLAG(IS_ANDROID)

MockMediaSinksObserver::MockMediaSinksObserver(MediaRouter* router,
                                               const MediaSource& source,
                                               const url::Origin& origin)
    : MediaSinksObserver(router, source, origin) {}
MockMediaSinksObserver::~MockMediaSinksObserver() = default;

MockMediaRoutesObserver::MockMediaRoutesObserver(MediaRouter* router)
    : MediaRoutesObserver(router) {}
MockMediaRoutesObserver::~MockMediaRoutesObserver() = default;

MockPresentationConnectionProxy::MockPresentationConnectionProxy() = default;
MockPresentationConnectionProxy::~MockPresentationConnectionProxy() = default;

}  // namespace media_router
