// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/register_protocol_handler_permission_request.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/permissions/permission_request.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace custom_handlers {

class RegisterProtocolHandlerPermissionRequestTest : public testing::Test {
 protected:
  // IsDuplicateOf() never touches the registry, so no registry is needed.
  std::unique_ptr<permissions::PermissionRequest> CreateRequest(
      const std::string& protocol,
      const GURL& handler_url,
      const url::Origin& requesting_origin =
          url::Origin::Create(GURL("https://requester.example"))) {
    return std::make_unique<RegisterProtocolHandlerPermissionRequest>(
        /*registry=*/nullptr,
        ProtocolHandler::CreateProtocolHandler(protocol, handler_url),
        requesting_origin, base::ScopedClosureRunner());
  }

 private:
  // ProtocolHandler must be created on the UI thread.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(RegisterProtocolHandlerPermissionRequestTest, SameHandlerIsDuplicate) {
  auto a = CreateRequest("web+test", GURL("https://requester.example/a?%s"));
  auto b = CreateRequest("web+test", GURL("https://requester.example/a?%s"));
  EXPECT_TRUE(a->IsDuplicateOf(b.get()));
  EXPECT_TRUE(b->IsDuplicateOf(a.get()));
}

// The permission request manager hands a duplicate the decision made on the
// request it duplicates, so two requests for different handlers must never be
// duplicates: accepting one would silently register the other.
TEST_F(RegisterProtocolHandlerPermissionRequestTest,
       SameSchemeDifferentPathIsNotDuplicate) {
  auto a = CreateRequest("web+test", GURL("https://requester.example/a?%s"));
  auto b = CreateRequest("web+test", GURL("https://requester.example/b?%s"));
  EXPECT_FALSE(a->IsDuplicateOf(b.get()));
  EXPECT_FALSE(b->IsDuplicateOf(a.get()));
}

// Since the request is attributed to the requesting frame rather than to the
// handler URL, cross-origin handlers (allowed for extensions) share the same
// requesting origin and must still not be duplicates.
TEST_F(RegisterProtocolHandlerPermissionRequestTest,
       SameSchemeDifferentHandlerOriginIsNotDuplicate) {
  auto a = CreateRequest("geo", GURL("https://a.example/?q=%s"));
  auto b = CreateRequest("geo", GURL("https://b.example/?q=%s"));
  EXPECT_FALSE(a->IsDuplicateOf(b.get()));
  EXPECT_FALSE(b->IsDuplicateOf(a.get()));
}

TEST_F(RegisterProtocolHandlerPermissionRequestTest,
       DifferentSchemeIsNotDuplicate) {
  auto a = CreateRequest("web+foo", GURL("https://requester.example/?q=%s"));
  auto b = CreateRequest("web+bar", GURL("https://requester.example/?q=%s"));
  EXPECT_FALSE(a->IsDuplicateOf(b.get()));
  EXPECT_FALSE(b->IsDuplicateOf(a.get()));
}

TEST_F(RegisterProtocolHandlerPermissionRequestTest,
       DifferentRequestingOriginIsNotDuplicate) {
  const GURL handler_url("https://handler.example/?q=%s");
  auto a = CreateRequest("geo", handler_url,
                         url::Origin::Create(GURL("https://one.example")));
  auto b = CreateRequest("geo", handler_url,
                         url::Origin::Create(GURL("https://two.example")));
  EXPECT_FALSE(a->IsDuplicateOf(b.get()));
  EXPECT_FALSE(b->IsDuplicateOf(a.get()));
}

}  // namespace custom_handlers
