// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_handler.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void DummyCommand(const Status& status,
                  const base::Value::Dict& params,
                  const std::string& session_id,
                  const CommandCallback& callback) {
  callback.Run(status, std::make_unique<base::Value>(1), "session_id", false);
}

void OnResponse(net::HttpServerResponseInfo* response_to_set,
                std::unique_ptr<net::HttpServerResponseInfo> response) {
  *response_to_set = *response;
}

}  // namespace

TEST(HttpHandlerTest, HandleOutsideOfBaseUrl) {
  HttpHandler handler("base/url/");
  net::HttpServerRequestInfo request;
  request.method = "get";
  request.path = "base/path";
  request.data = "body";
  net::HttpServerResponseInfo response;
  handler.Handle(request, base::BindRepeating(&OnResponse, &response));
  ASSERT_EQ(net::HTTP_BAD_REQUEST, response.status_code());
}

TEST(HttpHandlerTest, HandleUnknownCommand) {
  HttpHandler handler("/");
  net::HttpServerRequestInfo request;
  request.method = "get";
  request.path = "/path";
  net::HttpServerResponseInfo response;
  handler.Handle(request, base::BindRepeating(&OnResponse, &response));
  ASSERT_EQ(net::HTTP_NOT_FOUND, response.status_code());
}

TEST(HttpHandlerTest, HandleNewSession) {
  HttpHandler handler("/base/");
  handler.command_map_ = std::make_unique<HttpHandler::CommandMap>();
  handler.command_map_->push_back(
      CommandMapping(kPost, internal::kNewSessionPathPattern,
                     base::BindRepeating(&DummyCommand, Status(kOk))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/base/session";
  request.data = "{}";
  net::HttpServerResponseInfo response;
  handler.Handle(request, base::BindRepeating(&OnResponse, &response));
  ASSERT_EQ(net::HTTP_OK, response.status_code());
  base::Value::Dict body;
  body.Set("status", kOk);
  body.Set("value", 1);
  body.Set("sessionId", "session_id");
  std::string json;
  base::JSONWriter::Write(body, &json);
  ASSERT_EQ(json, response.body());
}

TEST(HttpHandlerTest, HandleInvalidPost) {
  HttpHandler handler("/");
  handler.command_map_->push_back(CommandMapping(
      kPost, "path", base::BindRepeating(&DummyCommand, Status(kOk))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/path";
  request.data = "should be a dictionary";
  net::HttpServerResponseInfo response;
  handler.Handle(request, base::BindRepeating(&OnResponse, &response));
  ASSERT_EQ(net::HTTP_BAD_REQUEST, response.status_code());
}

TEST(HttpHandlerTest, HandleUnimplementedCommand) {
  HttpHandler handler("/");
  handler.command_map_->push_back(CommandMapping(
      kPost, "path",
      base::BindRepeating(&DummyCommand, Status(kUnknownCommand))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/path";
  request.data = "{}";
  net::HttpServerResponseInfo response;
  handler.Handle(request, base::BindRepeating(&OnResponse, &response));
  ASSERT_EQ(net::HTTP_NOT_IMPLEMENTED, response.status_code());
}

TEST(HttpHandlerTest, HandleCommand) {
  HttpHandler handler("/");
  handler.command_map_->push_back(CommandMapping(
      kPost, "path", base::BindRepeating(&DummyCommand, Status(kOk))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/path";
  request.data = "{}";
  net::HttpServerResponseInfo response;
  handler.Handle(request, base::BindRepeating(&OnResponse, &response));
  ASSERT_EQ(net::HTTP_OK, response.status_code());
  base::Value::Dict body;
  body.Set("status", kOk);
  body.Set("value", 1);
  body.Set("sessionId", "session_id");
  std::string json;
  base::JSONWriter::Write(body, &json);
  ASSERT_EQ(json, response.body());
}

TEST(HttpHandlerTest, StandardResponse_ErrorNoMessage) {
  HttpHandler handler("/");
  Status status = Status(kUnexpectedAlertOpen);
  ASSERT_NO_FATAL_FAILURE(handler.PrepareStandardResponse(
      "not used", status, std::make_unique<base::Value>(), "1234"));
}

TEST(MatchesCommandTest, DiffMethod) {
  CommandMapping command(kPost, "path",
                         base::BindRepeating(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::Value::Dict params;
  ASSERT_FALSE(internal::MatchesCommand(
      "get", "path", command, &session_id, &params));
  ASSERT_TRUE(session_id.empty());
  ASSERT_EQ(0u, params.size());
}

TEST(MatchesCommandTest, DiffPathLength) {
  CommandMapping command(kPost, "path/path",
                         base::BindRepeating(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::Value::Dict params;
  ASSERT_FALSE(internal::MatchesCommand(
      "post", "path", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      "post", std::string(), command, &session_id, &params));
  ASSERT_FALSE(
      internal::MatchesCommand("post", "/", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      "post", "path/path/path", command, &session_id, &params));
}

TEST(MatchesCommandTest, DiffPaths) {
  CommandMapping command(kPost, "path/apath",
                         base::BindRepeating(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::Value::Dict params;
  ASSERT_FALSE(internal::MatchesCommand(
      "post", "path/bpath", command, &session_id, &params));
}

TEST(MatchesCommandTest, Substitution) {
  CommandMapping command(kPost, "path/:sessionId/space/:a/:b",
                         base::BindRepeating(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::Value::Dict params;
  ASSERT_TRUE(internal::MatchesCommand(
      "post", "path/1/space/2/3", command, &session_id, &params));
  ASSERT_EQ("1", session_id);
  ASSERT_EQ(2u, params.size());
  const std::string* param = params.FindString("a");
  ASSERT_TRUE(param);
  ASSERT_EQ("2", *param);
  param = params.FindString("b");
  ASSERT_TRUE(param);
  ASSERT_EQ("3", *param);
}

TEST(MatchesCommandTest, DecodeEscape) {
  CommandMapping command(kPost, "path/:sessionId/attribute/:xyz",
                         base::BindRepeating(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::Value::Dict params;
  ASSERT_TRUE(internal::MatchesCommand(
      "post", "path/123/attribute/xyz%2Furl%7Ce%3A%40v",
      command, &session_id, &params));
  const std::string* param = params.FindString("xyz");
  ASSERT_TRUE(param);
  ASSERT_EQ("xyz/url|e:@v", *param);
}

TEST(MatchesCommandTest, DecodePercent) {
  CommandMapping command(kPost, "path/:xyz",
                         base::BindRepeating(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::Value::Dict params;
  ASSERT_TRUE(internal::MatchesCommand(
      "post", "path/%40a%%b%%c%%%%", command, &session_id, &params));
  const std::string* param = params.FindString("xyz");
  ASSERT_TRUE(param);
  ASSERT_EQ("@a%b%c%%", *param);
}
