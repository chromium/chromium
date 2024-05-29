// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_handler.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/server/http_server.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainsRegex;
using testing::Eq;
using testing::Field;
using testing::Optional;
using testing::Pointee;
using testing::Property;

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

template <int Code>
testing::AssertionResult StatusCodeIs(const Status& status) {
  if (status.code() == Code) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

testing::AssertionResult StatusOk(const Status& status) {
  return StatusCodeIs<kOk>(status);
}

std::string ToString(const base::Value::Dict& dict) {
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(dict, &json));
  return json;
}

class MockHttpServer : public virtual HttpServerInterface {
 public:
  MOCK_METHOD(void, Close, (int connection_id), (override));
  MOCK_METHOD(void,
              AcceptWebSocket,
              (int connection_id, const net::HttpServerRequestInfo& request),
              (override));
  MOCK_METHOD(void,
              SendOverWebSocket,
              (int connection_id, const std::string& data),
              (override));
  MOCK_METHOD(void,
              SendResponse,
              (int connection_id,
               const net::HttpServerResponseInfo& response,
               const net::NetworkTrafficAnnotationTag& traffic_annotation),
              (override));
};

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

TEST(ParseBidiCommandTest, WellFormed) {
  std::string data =
      "{\"id\": 12, \"method\": \"some\", \"params\":{\"one\": 2}}";
  base::Value::Dict parsed;
  EXPECT_TRUE(StatusOk(internal::ParseBidiCommand(data, parsed)));
  EXPECT_THAT(parsed.FindInt("id"), Optional(Eq(12)));
  EXPECT_THAT(parsed.FindString("method"), Pointee(Eq("some")));
  base::Value::Dict* params = parsed.FindDict("params");
  ASSERT_NE(nullptr, params);
  ASSERT_THAT(params->FindInt("one"), Optional(Eq(2)));
}

TEST(ParseBidiCommandTest, MaxId) {
  std::string data =
      "{\"id\": 9007199254740991, \"method\": \"some\", \"params\":{}}";
  base::Value::Dict parsed;
  EXPECT_TRUE(StatusOk(internal::ParseBidiCommand(data, parsed)));
  EXPECT_THAT(parsed.FindDouble("id"), Optional(Eq(9007199254740991L)));
  EXPECT_THAT(parsed.FindString("method"), Pointee(Eq("some")));
  EXPECT_NE(nullptr, parsed.FindDict("params"));
}

TEST(ParseBidiCommandTest, MalformedJson) {
  std::string data =
      "{\"id\": 9007199254740991, \"method\": \"some\", \"params\":{";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("unable\\s+to\\s+parse"));
  EXPECT_TRUE(parsed.empty());
}

TEST(ParseBidiCommandTest, NotDictionary) {
  std::string data = "\"some string\"";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("dictionary\\s+is\\s+expected"));
  EXPECT_TRUE(parsed.empty());
}

TEST(ParseBidiCommandTest, NoId) {
  std::string data = "{\"method\": \"some\", \"params\":{}}";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("no\\s+'id'"));
}

TEST(ParseBidiCommandTest, WrongIdType) {
  std::string data = "{\"id\": {}, \"method\": \"some\", \"params\":{}}";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("no\\s+'id'"));
}

TEST(ParseBidiCommandTest, NoMethod) {
  std::string data = "{\"id\": 625, \"params\":{}}";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("no\\s+'method'"));
}

TEST(ParseBidiCommandTest, WrongMethodType) {
  std::string data = "{\"id\": 4, \"method\": {}, \"params\":{}}";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("no\\s+'method'"));
}

TEST(ParseBidiCommandTest, NoParams) {
  std::string data = "{\"id\": 625, \"method\":\"some\"}";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("no\\s+'params'"));
}

TEST(ParseBidiCommandTest, WrongParamsType) {
  std::string data = "{\"id\": 4, \"method\": \"some\", \"params\": 5}";
  base::Value::Dict parsed;
  Status status = internal::ParseBidiCommand(data, parsed);
  EXPECT_EQ(kInvalidArgument, status.code());
  EXPECT_THAT(status.message(), ContainsRegex("no\\s+'params'"));
}

TEST(CreateBidiErrorResponse, WithId) {
  Status error_status{kUnknownCommand, "this game has no name"};
  base::Value::Dict response =
      internal::CreateBidiErrorResponse(error_status, base::Value(121));
  EXPECT_THAT(response.FindString("type"), Pointee(Eq("error")));
  EXPECT_THAT(response.FindInt("id"), Optional(Eq(121)));
  EXPECT_THAT(response.FindString("error"), Pointee(Eq("unknown command")));
  EXPECT_THAT(response.FindString("message"),
              Pointee(ContainsRegex("this game has no name")));
  EXPECT_EQ(nullptr, response.Find("dropped_key"));
}

TEST(CreateBidiErrorResponse, NoId) {
  Status error_status{kUnknownCommand, "this game has no name"};
  base::Value::Dict response = internal::CreateBidiErrorResponse(error_status);
  EXPECT_THAT(response.FindString("type"), Pointee(Eq("error")));
  EXPECT_THAT(response.FindInt("id"), Eq(std::nullopt));
  EXPECT_THAT(response.FindString("error"), Pointee(Eq("unknown command")));
  EXPECT_THAT(response.FindString("message"),
              Pointee(ContainsRegex("this game has no name")));
}

class WebSocketMessageTest : public testing::Test {
 public:
  void Echo(const base::Value::Dict& params,
            const std::string& session_id,
            const CommandCallback& callback) {
    base::Value::Dict response = params.Clone();
    response.Set("is_response", true);
    callback.Run(Status{kOk},
                 std::make_unique<base::Value>(std::move(response)), session_id,
                 true);
  }

  Command EchoClosure() {
    return base::BindRepeating(&WebSocketMessageTest::Echo,
                               base::Unretained(this));
  }

  Command FailClosure(StatusCode code, const std::string& message) {
    return base::BindRepeating(
        [](StatusCode code, std::string message,
           const base::Value::Dict& params, const std::string& session_id,
           const CommandCallback& callback) {
          callback.Run(Status{code, message}, nullptr, session_id, true);
        },
        code, message);
  }

  Command SuccessNoResultClosure() {
    return base::BindRepeating([](const base::Value::Dict& params,
                                  const std::string& session_id,
                                  const CommandCallback& callback) {
      callback.Run(Status{kOk}, nullptr, session_id, true);
    });
  }

  Command SuccessEmptyResultClosure() {
    return base::BindRepeating([](const base::Value::Dict& params,
                                  const std::string& session_id,
                                  const CommandCallback& callback) {
      callback.Run(Status{kOk},
                   std::make_unique<base::Value>(base::Value::Type::DICT),
                   session_id, true);
    });
  }

  Command SessionCreatedClosure(std::string session_id) {
    return base::BindRepeating(
        [](std::string created_session_id, const base::Value::Dict& params,
           const std::string& session_id, const CommandCallback& callback) {
          callback.Run(Status{kOk},
                       std::make_unique<base::Value>(base::Value::Type::DICT),
                       created_session_id, true);
        },
        std::move(session_id));
  }

 protected:
  void SetUp() override {
    handler = std::make_unique<HttpHandler>("/");
    handler->io_task_runner_ = task_environment.GetMainThreadTaskRunner();
    handler->cmd_task_runner_ = task_environment.GetMainThreadTaskRunner();
  }

  void TearDown() override { handler.reset(); }

  // Register connection in HttpHandler
  // If session_id is omitted the connection is registered as unbound.
  void AddConnection(int connection_id, const std::string& session_id = "") {
    handler->connection_session_map_.insert(
        std::make_pair(connection_id, session_id));
    handler->session_connection_map_[session_id].push_back(connection_id);
  }

  void AddStaticCommand(std::string name, Command command) {
    handler->static_bidi_command_map_.emplace(std::move(name),
                                              std::move(command));
  }

  void AddSessionCommand(std::string name, Command command) {
    handler->session_bidi_command_map_.emplace(std::move(name),
                                               std::move(command));
  }

  void SetForwardingCommand(Command command) {
    handler->forward_session_command_ = std::move(command);
  }

  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<HttpHandler> handler;
};

TEST_F(WebSocketMessageTest, UnknownSessionNoId) {
  // Verify that the unknow session error is handled first.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  Status expected_error{kInvalidSessionId, "session not found"};
  std::string expected_response =
      ToString(internal::CreateBidiErrorResponse(expected_error));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(1), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 1, "not used");
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, UnknownSessionNoIdIsPostedToIO) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  handler->OnWebSocketMessage(&http_server, 1, "not used");
}

TEST_F(WebSocketMessageTest, UnknownSessionWithId) {
  // Verify that the unknow session error is handled first.
  // The response must contain the id of the corresponding command.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  Status expected_error{kInvalidSessionId, "session not found"};
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(15)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(3), Eq(expected_response)));
  std::string incoming = "{\"method\": \"some\", \"id\": 15, \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, UnknownSessionWithIdIsPostedToIO) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  std::string incoming = "{\"method\": \"some\", \"id\": 15, \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, SessionCommandWithIdUnboundConnection) {
  // Well formed session command arriving over an unbound connection must be
  // handled with "invalid session id" error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  Status expected_error{kInvalidSessionId, "session not found"};
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(15)));
  // The connection is unbound
  AddConnection(3);
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(3), Eq(expected_response)));
  std::string incoming =
      "{\"method\": \"script.evaluate\", \"id\": 15, \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandWithIdPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3);
  std::string incoming =
      "{\"method\": \"script.evaluate\", \"id\": 15, \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, SessionCommandNoIdUnboundConnection) {
  // Verify that missing "id" is checked before connection affinity with any
  // session.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  std::string incoming = "{\"method\": \"script.evaluate\", \"params\": {}}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  // missing "id" is an "invalid argument" error
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response =
      ToString(internal::CreateBidiErrorResponse(expected_error));
  AddConnection(3);
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(3), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 3, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandNoIdPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3);
  std::string incoming = "{\"method\": \"script.evaluate\", \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, SessionCommandNoIdBoundConnection) {
  // Verify that the missing command "id" is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(4, "some_session");
  std::string incoming = "{\"method\": \"script.evaluate\", \"params\": {}}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response =
      ToString(internal::CreateBidiErrorResponse(expected_error));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(4), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 4, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandNoIdPostedToIOBoundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3, "some_session");
  std::string incoming = "{\"method\": \"script.evaluate\", \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, NoMethodUnboundConnection) {
  // Verify that the missing "method" is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(5);
  std::string incoming = "{\"id\": 61, \"params\": {}}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(61)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(5), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 5, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, NoMethodPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3);
  std::string incoming = "{\"id\": 61, \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, NoMethodBoundConnection) {
  // Verify that the missing "method" is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(5, "some_session");
  std::string incoming = "{\"id\": 61, \"params\": {}}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(61)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(5), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 5, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, NoMethodPostedToIOBoundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3, "some_session");
  std::string incoming = "{\"id\": 61, \"params\": {}}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, SessionCommandNoParamsUnboundConnection) {
  // Verify that the missing command params are treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6);
  std::string incoming = "{\"method\": \"script.evaluate\", \"id\": 18}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(18)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(6), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 6, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest,
       SessionCommandNoParamsPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3);
  std::string incoming = "{\"method\": \"script.evaluate\", \"id\": 18}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, SessionCommandNoParamsBoundConnection) {
  // Verify that the missing command params are treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6, "some_session");
  std::string incoming = "{\"method\": \"script.evaluate\", \"id\": 18}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(18)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(6), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 6, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandNoParamsPostedToIOBoundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3, "some_session");
  std::string incoming = "{\"method\": \"script.evaluate\", \"id\": 18}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, MalformedJson) {
  // Verify that the malformed JSON is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6, "some_session");
  std::string incoming = "}{";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_TRUE(expected_error.IsError());
  std::string expected_response =
      ToString(internal::CreateBidiErrorResponse(expected_error));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(6), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 6, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, MalformedJsonPostedToIO) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6, "some_session");
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  // The message contains no id, no method and no params
  handler->OnWebSocketMessage(&http_server, 6, "}{");
}

TEST_F(WebSocketMessageTest, UnknownCommandUnboundConnection) {
  // Verify that any unknown static command is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(7);
  std::string incoming =
      "{\"method\": \"abracadabra\", \"id\": 19, \"params\": {}}";
  Status expected_error = {kUnknownCommand, "abracadabra"};
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(19)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(7), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 7, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, UnknownCommandIsPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(7);
  std::string incoming =
      "{\"method\": \"abracadabra\", \"id\": 19, \"params\": {}}";
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  handler->OnWebSocketMessage(&http_server, 7, incoming);
}

TEST_F(WebSocketMessageTest, UnknownCommandBoundConnection) {
  // Verify that any unknown command is forwarded to BiDiMapper in the case if
  // the connection is bound to a BiDi session.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(7, "some_session");
  std::string incoming =
      "{\"method\": \"abracadabra\", \"id\": 19, \"params\": {}}";
  bool invoked = false;
  SetForwardingCommand(base::BindRepeating(
      [](bool* invoked, const base::Value::Dict& params,
         const std::string& session_id, const CommandCallback&) {
        *invoked = true;
        EXPECT_EQ("some_session", session_id);
        EXPECT_EQ(7, params.FindDouble("connectionId").value_or(-1));
        EXPECT_THAT(params.FindStringByDottedPath("bidiCommand.method"),
                    Pointee(Eq("abracadabra")));
        EXPECT_THAT(params.FindDoubleByDottedPath("bidiCommand.id"),
                    Optional(Eq(19)));
      },
      base::Unretained(&invoked)));
  handler->OnWebSocketMessage(&http_server, 7, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(invoked);
}

TEST_F(WebSocketMessageTest, StaticCommandNoIdUnboundConnection) {
  // Verify that missing "id" is checked before static command invocation
  base::RunLoop run_loop;
  AddStaticCommand("echo", EchoClosure());
  std::string incoming = "{\"method\": \"echo\", \"params\": {}}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  // missing "id" is an "invalid argument" error
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response =
      ToString(internal::CreateBidiErrorResponse(expected_error));
  AddConnection(3);
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(3), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 3, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, StaticCommandNoIdPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  AddStaticCommand("echo", EchoClosure());
  std::string incoming = "{\"method\": \"echo\", \"params\": {}}";
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3);
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, StaticCommandNoIdBoundConnection) {
  // Verify that missing "id" is checked before static command invocation
  base::RunLoop run_loop;
  AddStaticCommand("echo", EchoClosure());
  std::string incoming = "{\"method\": \"echo\", \"params\": {}}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  // missing "id" is an "invalid argument" error
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response =
      ToString(internal::CreateBidiErrorResponse(expected_error));
  AddConnection(3, "some_session");
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(3), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 3, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, StaticCommandNoIdPostedToBoundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  AddStaticCommand("echo", EchoClosure());
  std::string incoming = "{\"method\": \"echo\", \"params\": {}}";
  MockHttpServer http_server;
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3, "some_session");
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, StaticCommandNoParamsUnboundConnection) {
  // Verify that the missing command params are treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6);
  AddStaticCommand("echo", EchoClosure());
  std::string incoming = "{\"method\": \"echo\", \"id\": 18}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(18)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(6), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 6, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, StaticCommandNoParamsPostedToIOUnboundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6);
  AddStaticCommand("echo", EchoClosure());
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3);
  std::string incoming = "{\"method\": \"echo\", \"id\": 18}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, StaticCommandNoParamsBoundConnection) {
  // Verify that the missing command params are treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(6, "some_session");
  AddStaticCommand("echo", EchoClosure());
  std::string incoming = "{\"method\": \"echo\", \"id\": 18}";
  base::Value::Dict parsed;
  Status expected_error = internal::ParseBidiCommand(incoming, parsed);
  EXPECT_EQ(kInvalidArgument, expected_error.code());
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(18)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(6), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 6, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, StaticCommandNoParamsPostedToIOBoundConnection) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(3, "some_session");
  AddStaticCommand("echo", EchoClosure());
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  AddConnection(3, "some_session");
  std::string incoming = "{\"method\": \"echo\", \"id\": 18}";
  handler->OnWebSocketMessage(&http_server, 3, incoming);
}

TEST_F(WebSocketMessageTest, KnownStaticCommandReturnsSuccess) {
  // Verify that the response from a successful static command is sent over the
  // web socket.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(8);
  AddStaticCommand("echo", EchoClosure());
  std::string incoming =
      "{\"method\": \"echo\", \"id\": 20, \"params\": {\"a\": 1}}";
  base::Value::Dict expected_response;
  base::Value::Dict parsed;
  EXPECT_TRUE(StatusOk(internal::ParseBidiCommand(incoming, parsed)));
  parsed.Set("is_response", true);
  expected_response.Set("type", "success");
  expected_response.Set("id", parsed.FindInt("id").value_or(-1));
  expected_response.Set("result", std::move(parsed));
  std::string expected_response_message = ToString(expected_response);
  EXPECT_CALL(http_server,
              SendOverWebSocket(Eq(8), Eq(expected_response_message)));
  handler->OnWebSocketMessage(&http_server, 8, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, KnownStaticCommandReturnsError) {
  // Verify that the error message from a failed static command is sent over the
  // web socket.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(9);
  AddStaticCommand("fail",
                   FailClosure(kInvalidSelector, "this game has no name"));
  std::string incoming =
      "{\"method\": \"fail\", \"id\": 21, \"params\": {\"a\": 1}}";
  Status expected_error = Status{kInvalidSelector, "this game has no name"};
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(21)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(9), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 9, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, KnownStaticCommandResponseIsPostedToIO) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(8);
  AddStaticCommand("echo", EchoClosure());
  std::string incoming =
      "{\"method\": \"echo\", \"id\": 20, \"params\": {\"a\": 1}}";
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  handler->OnWebSocketMessage(&http_server, 8, incoming);
}

TEST_F(WebSocketMessageTest, SessionCommandReturnsSuccess) {
  // Verify that the response from a successful session command is sent over
  // the web socket.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(8, "some_session");
  SetForwardingCommand(SuccessNoResultClosure());
  std::string incoming =
      "{\"method\": \"echo\", \"id\": 20, \"params\": {\"a\": 1}}";
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  handler->OnWebSocketMessage(&http_server, 8, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandNoReturnValue) {
  // Verify that no response is sent to the user if the session command was
  // forwarded to BiDiMapper.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(8, "some_session");
  SetForwardingCommand(SuccessNoResultClosure());
  std::string incoming =
      "{\"method\": \"echo\", \"id\": 20, \"params\": {\"a\": 1}}";
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  handler->OnWebSocketMessage(&http_server, 8, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandReturnsError) {
  // Verify that the error from a failed session command is sent over the
  // web socket.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(9, "some_session");
  Command fail = base::BindRepeating([](const base::Value::Dict& params,
                                        const std::string& session_id,
                                        const CommandCallback& callback) {
    base::Value::Dict response;
    if (params.FindDoubleByDottedPath("bidiCommand.id")) {
      response.Set("id", *params.FindDoubleByDottedPath("bidiCommand.id"));
    }
    callback.Run(Status{kJavaScriptError, "this game has no name"},
                 std::make_unique<base::Value>(std::move(response)), session_id,
                 true);
  });
  SetForwardingCommand(std::move(fail));
  std::string incoming =
      "{\"method\": \"fail\", \"id\": 21, \"params\": {\"a\": 1}}";
  Status expected_error = Status{kJavaScriptError, "this game has no name"};
  std::string expected_response = ToString(
      internal::CreateBidiErrorResponse(expected_error, base::Value(21)));
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(9), Eq(expected_response)));
  handler->OnWebSocketMessage(&http_server, 9, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketMessageTest, SessionCommandResponseIsPostedToIO) {
  // Verify that the response is properly posted to the IO thread.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(8);
  Command echo = base::BindRepeating([](const base::Value::Dict& params,
                                        const std::string& session_id,
                                        const CommandCallback& callback) {
    base::Value::Dict response;
    if (params.FindDict("bidiCommand")) {
      response = params.FindDict("bidiCommand")->Clone();
    }
    response.Set("is_response", true);
    callback.Run(Status{kOk},
                 std::make_unique<base::Value>(std::move(response)), session_id,
                 true);
  });
  AddStaticCommand("echo", std::move(echo));
  std::string incoming =
      "{\"method\": \"echo\", \"id\": 20, \"params\": {\"a\": 1}}";
  EXPECT_CALL(http_server, SendOverWebSocket(_, _)).Times(0);
  handler->OnWebSocketMessage(&http_server, 8, incoming);
}

TEST_F(WebSocketMessageTest, StaticCommandOnSessionBoundConnection) {
  // Verify that static commands are not forwarded to BiDiMapper.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(9, "some_session");
  auto register_invocation = base::BindRepeating(
      [](bool* invoked, const base::Value::Dict& params,
         const std::string& session_id, const CommandCallback& callback) {
        *invoked = true;
        callback.Run(Status{kOk}, std::make_unique<base::Value>(params.Clone()),
                     session_id, true);
      });
  bool static_is_invoked = false;
  AddStaticCommand("static_cmd", base::BindRepeating(register_invocation,
                                                     &static_is_invoked));
  bool session_is_invoked = false;
  SetForwardingCommand(
      base::BindRepeating(register_invocation, &session_is_invoked));
  std::string incoming =
      "{\"method\": \"static_cmd\", \"id\": 20, \"params\": {\"a\": 1}}";
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(9), _));
  handler->OnWebSocketMessage(&http_server, 9, incoming);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(static_is_invoked);
  EXPECT_FALSE(session_is_invoked);
}

TEST_F(WebSocketMessageTest, SessionNew) {
  // Verify that unbound connection becomes session bound after session.new
  MockHttpServer http_server;
  // Initially connection #8 is unbound
  AddConnection(8);
  AddStaticCommand("session.new", SessionCreatedClosure("gray_session"));
  SetForwardingCommand(SuccessEmptyResultClosure());
  // Check that connection #8 is not open
  {
    Status expected_error{kInvalidSessionId, "session not found"};
    std::string expected_response_str = ToString(
        internal::CreateBidiErrorResponse(expected_error, base::Value(23)));
    EXPECT_CALL(http_server, SendOverWebSocket(8, expected_response_str));
    std::string incoming =
        "{\"method\": \"script.evaluate\", \"id\": 23, \"params\": {}}";
    base::RunLoop run_loop;
    handler->OnWebSocketMessage(&http_server, 8, incoming);
    task_environment.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::Value::Dict expected_response;
    expected_response.Set("id", 24);
    expected_response.Set("type", "success");
    expected_response.Set("result", base::Value::Dict());
    std::string expected_response_str = ToString(expected_response);
    EXPECT_CALL(http_server, SendOverWebSocket(8, expected_response_str));
    std::string incoming =
        "{\"method\": \"session.new\", \"id\": 24, \"params\": {}}";
    base::RunLoop run_loop;
    handler->OnWebSocketMessage(&http_server, 8, incoming);
    task_environment.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  // Check that connection #8 is bound to a new session
  {
    base::Value::Dict expected_response;
    expected_response.Set("id", 25);
    expected_response.Set("type", "success");
    expected_response.Set("result", base::Value::Dict());
    std::string expected_response_str = ToString(expected_response);
    EXPECT_CALL(http_server, SendOverWebSocket(8, expected_response_str));
    std::string incoming =
        "{\"method\": \"script.evaluate\", \"id\": 25, \"params\": {}}";
    base::RunLoop run_loop;
    handler->OnWebSocketMessage(&http_server, 8, incoming);
    task_environment.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
}

TEST_F(WebSocketMessageTest, SessionEnd) {
  // Verify that all bound connection are closed after session.end
  MockHttpServer http_server;
  AddConnection(10, "red_session");
  AddSessionCommand("session.end", SuccessEmptyResultClosure());
  SetForwardingCommand(SuccessEmptyResultClosure());
  {
    base::Value::Dict expected_response;
    expected_response.Set("id", 21);
    expected_response.Set("type", "success");
    expected_response.Set("result", base::Value::Dict());
    std::string expected_response_str = ToString(expected_response);
    EXPECT_CALL(http_server, SendOverWebSocket(10, expected_response_str));
    std::string incoming =
        "{\"method\": \"session.end\", \"id\": 21, \"params\": {}}";
    base::RunLoop run_loop;
    handler->OnWebSocketMessage(&http_server, 10, incoming);
    task_environment.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
}

class WebSocketRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    handler = std::make_unique<HttpHandler>("/");
    handler->io_task_runner_ = task_environment.GetMainThreadTaskRunner();
    handler->cmd_task_runner_ = task_environment.GetMainThreadTaskRunner();
  }

  void TearDown() override { handler.reset(); }

  // Register connection in HttpHandler
  // If session_id is omitted the connection is registered as unbound.
  void AddConnection(int connection_id, const std::string& session_id = "") {
    handler->connection_session_map_.insert(
        std::make_pair(connection_id, session_id));
    handler->session_connection_map_[session_id].push_back(connection_id);
  }

  void AddStaticCommand(std::string name, Command command) {
    handler->static_bidi_command_map_.emplace(std::move(name),
                                              std::move(command));
  }

  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<HttpHandler> handler;
};

TEST_F(WebSocketRequestTest, UnknownPath) {
  // Verify that an unknown path is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  net::HttpServerRequestInfo rq;
  rq.path = "this/game/has/no/name";
  net::HttpServerResponseInfo expected_response(net::HTTP_BAD_REQUEST);
  expected_response.AddHeader(
      "X-WebSocket-Reject-Reason",
      "bad request received path this/game/has/no/name");
  EXPECT_CALL(http_server,
              SendResponse(Eq(2),
                           Property(&net::HttpServerResponseInfo::Serialize,
                                    Eq(expected_response.Serialize())),
                           _));
  handler->OnWebSocketRequest(&http_server, 2, rq);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketRequestTest, UnknownSession) {
  // Verify that an unknown session is treated as an error.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  net::HttpServerRequestInfo rq;
  rq.path = "session/unknown_session_id";
  net::HttpServerResponseInfo expected_response(net::HTTP_BAD_REQUEST);
  expected_response.AddHeader(
      "X-WebSocket-Reject-Reason",
      "bad request invalid session id unknown_session_id");
  EXPECT_CALL(http_server,
              SendResponse(Eq(1),
                           Property(&net::HttpServerResponseInfo::Serialize,
                                    Eq(expected_response.Serialize())),
                           _));
  handler->OnWebSocketRequest(&http_server, 1, rq);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketRequestTest, ConnectionAlreadyBound) {
  // Verify that a connection cannot be bound with a session twice.
  base::RunLoop run_loop;
  MockHttpServer http_server;
  AddConnection(3, "existing_session");
  net::HttpServerRequestInfo rq;
  rq.path = "session";
  net::HttpServerResponseInfo expected_response(net::HTTP_BAD_REQUEST);
  expected_response.AddHeader(
      "X-WebSocket-Reject-Reason",
      "connection is already bound to session_id=existing_session");
  EXPECT_CALL(http_server,
              SendResponse(Eq(3),
                           Property(&net::HttpServerResponseInfo::Serialize,
                                    Eq(expected_response.Serialize())),
                           _));
  handler->OnWebSocketRequest(&http_server, 3, rq);
  task_environment.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WebSocketRequestTest, CreateUnboundConnection) {
  // Verify that an unbound connection can be established.
  MockHttpServer http_server;
  net::HttpServerRequestInfo rq;
  rq.path = "session";
  EXPECT_CALL(http_server, AcceptWebSocket(Eq(3), _));
  handler->OnWebSocketRequest(&http_server, 3, rq);
  {
    base::RunLoop run_loop;
    task_environment.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  bool invoked = false;
  Command cmd = base::BindRepeating(
      [](bool* invoked, const base::Value::Dict& params,
         const std::string& session_id, const CommandCallback& callback) {
        *invoked = true;
        callback.Run(Status{kOk}, std::make_unique<base::Value>(params.Clone()),
                     session_id, true);
      },
      &invoked);
  AddStaticCommand("cmd", std::move(cmd));
  std::string incoming = "{\"method\": \"cmd\", \"id\": 20, \"params\": {}}";
  EXPECT_CALL(http_server, SendOverWebSocket(Eq(3), _));
  handler->OnWebSocketMessage(&http_server, 3, incoming);
  {
    base::RunLoop run_loop;
    task_environment.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(invoked);
}
