// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus_xdg {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

constexpr char kConnectionName[] = ":1.2";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kTestInterface[] = "org.freedesktop.portal.Inhibit";
constexpr char kTestMethod[] = "Inhibit";
constexpr char kOldPortalRequestPath[] =
    "/org/freedesktop/portal/desktop/request/1_2/old";

// dbus::MockBus doesn't mock RemoveObjectProxy().
class TestBus : public dbus::MockBus {
 public:
  using dbus::MockBus::MockBus;
  MOCK_METHOD(bool,
              RemoveObjectProxy,
              (std::string_view, const dbus::ObjectPath&, base::OnceClosure),
              (override));

 private:
  ~TestBus() override = default;
};

class RequestTest : public testing::Test {
 protected:
  RequestTest()
      : bus_(base::MakeRefCounted<TestBus>(dbus::Bus::Options())),
        portal_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            bus_.get(),
            kPortalServiceName,
            dbus::ObjectPath(kPortalObjectPath))),
        request_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            bus_.get(),
            kPortalServiceName,
            dbus::ObjectPath(
                "/org/freedesktop/portal/desktop/request/1_2/t"))) {
    ON_CALL(*bus_, GetConnectionName()).WillByDefault(Return(kConnectionName));
    ON_CALL(*bus_, GetObjectProxy(kPortalServiceName, _))
        .WillByDefault([this](std::string_view, const dbus::ObjectPath& path) {
          requested_paths_.push_back(path);
          return request_proxy_.get();
        });
    ON_CALL(*bus_, RemoveObjectProxy(kPortalServiceName, _, _))
        .WillByDefault([this](std::string_view, const dbus::ObjectPath& path,
                              base::OnceClosure) {
          removed_paths_.push_back(path);
          return true;
        });
    ON_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault(
            [this](dbus::MethodCall* call, int,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              EXPECT_EQ(call->GetInterface(), kTestInterface);
              EXPECT_EQ(call->GetMember(), kTestMethod);
              dbus::MessageReader reader(call);
              uint32_t flags = 0;
              EXPECT_TRUE(reader.PopUint32(&flags));
              auto options = dbus_utils::ReadValue<Dictionary>(reader);
              ASSERT_TRUE(options.has_value());
              auto it = options->find("handle_token");
              ASSERT_NE(it, options->end());
              request_path_ =
                  dbus::ObjectPath(base::nix::XdgDesktopPortalRequestPath(
                      kConnectionName,
                      std::move(it->second).Take<std::string>().value_or("")));
              method_callback_ = std::move(callback);
            });
    ON_CALL(*request_proxy_,
            ConnectToSignal(kRequestInterface, "Response", _, _))
        .WillByDefault(
            [this](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback connected_callback) {
              signal_callback_ = std::move(signal_callback);
              std::move(connected_callback)
                  .Run(interface_name, signal_name, true);
            });
    ON_CALL(*request_proxy_, CallMethod(_, _, _))
        .WillByDefault([this](dbus::MethodCall* call, int,
                              dbus::ObjectProxy::ResponseCallback) {
          EXPECT_EQ(call->GetInterface(), kRequestInterface);
          EXPECT_EQ(call->GetMember(), "Close");
          closed_paths_.push_back(requested_paths_.back());
        });
  }

  ResponseCallback ResultCallback() {
    return base::BindLambdaForTesting(
        [this](Results results) { results_.push_back(std::move(results)); });
  }

  std::unique_ptr<Request> CreateRequest() {
    return std::make_unique<Request>(bus_, portal_proxy_.get(), kTestInterface,
                                     kTestMethod, Dictionary(),
                                     ResultCallback(), 8u);
  }

  std::unique_ptr<Request> CreateLongLivedRequest() {
    return Request::CreateLongLived(bus_, portal_proxy_.get(), kTestInterface,
                                    kTestMethod, Dictionary(), ResultCallback(),
                                    8u);
  }

  // Replies to the method call with `path`, or with an error if `path` is
  // nullopt.
  void ReplyToMethodCall(std::optional<dbus::ObjectPath> path) {
    ASSERT_FALSE(method_callback_.is_null());
    if (!path) {
      std::move(method_callback_).Run(nullptr, nullptr);
      return;
    }
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendObjectPath(*path);
    std::move(method_callback_).Run(response.get(), nullptr);
  }

  void EmitResponse(uint32_t response_code) {
    ASSERT_FALSE(signal_callback_.is_null());
    dbus::Signal signal(kRequestInterface, "Response");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(response_code);
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("{sv}", &array_writer);
    writer.CloseContainer(&array_writer);
    signal_callback_.Run(&signal);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;
  scoped_refptr<dbus::MockObjectProxy> request_proxy_;

  dbus::ObjectPath request_path_;
  dbus::ObjectProxy::ResponseOrErrorCallback method_callback_;
  dbus::ObjectProxy::SignalCallback signal_callback_;
  std::vector<Results> results_;
  std::vector<dbus::ObjectPath> requested_paths_;
  // Request object proxies removed from the bus. The mock request proxy never
  // runs Close() callbacks, so removals after Close() aren't included.
  std::vector<dbus::ObjectPath> removed_paths_;
  std::vector<dbus::ObjectPath> closed_paths_;
};

}  // namespace

// A regular request is over once the portal responds, so it isn't closed.
TEST_F(RequestTest, SuccessEndsRequest) {
  auto request = CreateRequest();
  ReplyToMethodCall(request_path_);
  EmitResponse(0);
  ASSERT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].has_value());
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));

  request.reset();
  EXPECT_TRUE(closed_paths_.empty());
}

// A long-lived request stays open after a successful response.
TEST_F(RequestTest, LongLivedSuccessKeepsRequestOpen) {
  auto request = CreateLongLivedRequest();
  ReplyToMethodCall(request_path_);
  EmitResponse(0);
  ASSERT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].has_value());
  EXPECT_TRUE(removed_paths_.empty());
  EXPECT_TRUE(closed_paths_.empty());

  request.reset();
  EXPECT_THAT(closed_paths_, ElementsAre(request_path_));
}

// Some portals never emit a response for long-lived requests.
TEST_F(RequestTest, LongLivedWithoutResponseIsClosed) {
  auto request = CreateLongLivedRequest();
  ReplyToMethodCall(request_path_);
  EXPECT_TRUE(results_.empty());

  request.reset();
  EXPECT_TRUE(results_.empty());
  EXPECT_THAT(closed_paths_, ElementsAre(request_path_));
}

TEST_F(RequestTest, LongLivedFailureResponseEndsRequest) {
  auto request = CreateLongLivedRequest();
  ReplyToMethodCall(request_path_);
  EmitResponse(2);
  ASSERT_EQ(results_.size(), 1u);
  EXPECT_EQ(results_[0],
            base::unexpected(ResponseError::kRequestCancelledOther));
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));

  request.reset();
  EXPECT_TRUE(closed_paths_.empty());
}

TEST_F(RequestTest, LongLivedMethodErrorEndsRequest) {
  auto request = CreateLongLivedRequest();
  ReplyToMethodCall(std::nullopt);
  ASSERT_EQ(results_.size(), 1u);
  EXPECT_EQ(results_[0], base::unexpected(ResponseError::kMethodCallFailed));
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));

  request.reset();
  EXPECT_TRUE(closed_paths_.empty());
}

// The portal may emit the response before the method call returns.
TEST_F(RequestTest, LongLivedSuccessBeforeReplyKeepsRequestOpen) {
  auto request = CreateLongLivedRequest();
  EmitResponse(0);
  ASSERT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].has_value());

  ReplyToMethodCall(request_path_);
  EXPECT_TRUE(closed_paths_.empty());

  request.reset();
  EXPECT_THAT(closed_paths_, ElementsAre(request_path_));
}

TEST_F(RequestTest, LongLivedFailureBeforeReplyEndsRequest) {
  auto request = CreateLongLivedRequest();
  EmitResponse(2);
  ASSERT_EQ(results_.size(), 1u);
  EXPECT_FALSE(results_[0].has_value());
  request.reset();

  ReplyToMethodCall(request_path_);
  EXPECT_TRUE(closed_paths_.empty());
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));
}

// If a long-lived request is destroyed before the method call returns, it's
// closed once the reply arrives.
TEST_F(RequestTest, LongLivedDestroyedBeforeReplyIsClosed) {
  auto request = CreateLongLivedRequest();
  request.reset();
  EXPECT_TRUE(closed_paths_.empty());

  ReplyToMethodCall(request_path_);
  EXPECT_TRUE(results_.empty());
  EXPECT_THAT(closed_paths_, ElementsAre(request_path_));
  EXPECT_TRUE(removed_paths_.empty());
}

TEST_F(RequestTest, LongLivedDestroyedBeforeFailedReply) {
  auto request = CreateLongLivedRequest();
  request.reset();

  ReplyToMethodCall(std::nullopt);
  EXPECT_TRUE(closed_paths_.empty());
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));
}

// Portals older than 0.9 ignore handle_token and return another path.
TEST_F(RequestTest, LongLivedOldPortalPathIsClosed) {
  const dbus::ObjectPath old_path(kOldPortalRequestPath);
  auto request = CreateLongLivedRequest();
  ReplyToMethodCall(old_path);
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));
  ASSERT_FALSE(requested_paths_.empty());
  EXPECT_EQ(requested_paths_.back(), old_path);

  request.reset();
  EXPECT_THAT(closed_paths_, ElementsAre(old_path));
}

TEST_F(RequestTest, LongLivedDestroyedBeforeOldPortalReplyIsClosed) {
  const dbus::ObjectPath old_path(kOldPortalRequestPath);
  auto request = CreateLongLivedRequest();
  request.reset();

  ReplyToMethodCall(old_path);
  EXPECT_THAT(closed_paths_, ElementsAre(old_path));
  EXPECT_THAT(removed_paths_, ElementsAre(request_path_));
}

}  // namespace dbus_xdg
