// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/mock_http_agent_factory.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "chrome/chrome_cleaner/http/http_agent.h"
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/http/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

// Class that provides a response based on how the MockHttpAgentConfig is
// configured.
class MockHttpResponse : public chrome_cleaner::HttpResponse {
 public:
  explicit MockHttpResponse(MockHttpAgentConfig* config) : config_(config) {
    DCHECK(config);
  }

  MockHttpResponse(const MockHttpResponse&) = delete;
  MockHttpResponse& operator=(const MockHttpResponse&) = delete;

  ~MockHttpResponse() override = default;

  // chrome_cleaner::HttpResponse:
  bool GetStatusCode(uint16_t* status_code) override {
    if (config_->GetCurrentCalls().get_status_code_succeeds) {
      *status_code = static_cast<uint16_t>(
          config_->GetCurrentCalls().get_status_code_result);
    }
    return config_->GetCurrentCalls().get_status_code_succeeds;
  }

  bool GetContentLength(bool* has_content_length,
                        uint32_t* content_length) override {
    ADD_FAILURE() << "This method should not be called.";
    return false;
  }

  bool GetContentType(bool* has_content_type,
                      std::wstring* content_type) override {
    ADD_FAILURE() << "This method should not be called.";
    return false;
  }

  bool HasData(bool* has_data) override {
    if (config_->GetCurrentCalls().has_data_succeeds)
      *has_data = !config_->GetCurrentCalls().read_data_result.empty();
    return config_->GetCurrentCalls().has_data_succeeds;
  }

  bool ReadData(char* buffer, uint32_t* count) override {
    MockHttpAgentConfig::Calls& calls = config_->GetCurrentCalls();

    bool succeeds = calls.read_data_succeeds_by_default;
    if (!calls.read_data_success_sequence.empty()) {
      succeeds = calls.read_data_success_sequence[0];
      calls.read_data_success_sequence.erase(
          calls.read_data_success_sequence.begin());
    }

    if (succeeds)
      config_->ReadData(buffer, count);
    return succeeds;
  }

 private:
  MockHttpAgentConfig* config_{nullptr};
};

// Class that acts as an HttpAgent based on how the MockHttpAgentConfig is
// configured.
class MockHttpAgent : public chrome_cleaner::HttpAgent {
 public:
  explicit MockHttpAgent(MockHttpAgentConfig* config) : config_(config) {
    DCHECK(config);
  }

  MockHttpAgent(const MockHttpAgent&) = delete;
  MockHttpAgent& operator=(const MockHttpAgent&) = delete;

  ~MockHttpAgent() override = default;

  // chrome_cleaner::HttpAgent:
  std::unique_ptr<chrome_cleaner::HttpResponse> Post(
      const std::wstring& host,
      uint16_t port,
      const std::wstring& path,
      bool secure,
      const std::wstring& extra_headers,
      const std::string& body,
      const net::NetworkTrafficAnnotationTag& /*traffic_annotation*/) override {
    const bool post_succeeds = config_->GetCurrentCalls().request_succeeds;
    MockHttpAgentConfig::RequestData post_data;
    post_data.host = host;
    post_data.port = port;
    post_data.path = path;
    post_data.secure = secure;
    post_data.extra_headers = extra_headers;
    post_data.body = body;
    config_->AddRequestData(post_data);

    if (post_succeeds)
      return std::make_unique<MockHttpResponse>(config_);
    return nullptr;
  }

  // chrome_cleaner::HttpAgent:
  std::unique_ptr<chrome_cleaner::HttpResponse> Get(
      const std::wstring& host,
      uint16_t port,
      const std::wstring& path,
      bool secure,
      const std::wstring& extra_headers,
      const net::NetworkTrafficAnnotationTag& /*traffic_annotation*/) override {
    const bool get_succeeds = config_->GetCurrentCalls().request_succeeds;
    MockHttpAgentConfig::RequestData get_data;
    get_data.host = host;
    get_data.port = port;
    get_data.path = path;
    get_data.secure = secure;
    get_data.extra_headers = extra_headers;
    config_->AddRequestData(get_data);

    if (get_succeeds)
      return std::make_unique<MockHttpResponse>(config_);
    return nullptr;
  }

 private:
  MockHttpAgentConfig* config_{nullptr};
};

}  // namespace

MockHttpAgentConfig::Calls::Calls(HttpStatus status)
    : get_status_code_result(status) {}

MockHttpAgentConfig::Calls::Calls(const Calls& other) = default;

MockHttpAgentConfig::Calls::~Calls() = default;

MockHttpAgentConfig::Calls& MockHttpAgentConfig::Calls::operator=(
    const MockHttpAgentConfig::Calls& other) = default;

MockHttpAgentConfig::RequestData::RequestData() = default;

MockHttpAgentConfig::RequestData::RequestData(const RequestData& other) =
    default;

MockHttpAgentConfig::RequestData::~RequestData() = default;

MockHttpAgentConfig::RequestData& MockHttpAgentConfig::RequestData::operator=(
    const MockHttpAgentConfig::RequestData& other) = default;

MockHttpAgentConfig::MockHttpAgentConfig() = default;

MockHttpAgentConfig::~MockHttpAgentConfig() = default;

size_t MockHttpAgentConfig::AddCalls(const Calls& calls) {
  calls_.push_back(calls);
  return calls_.size() - 1;
}

MockHttpAgentConfig::Calls& MockHttpAgentConfig::GetCurrentCalls() {
  if (current_index_ >= calls_.size()) {
    static Calls default_calls(HttpStatus::kOk);
    ADD_FAILURE() << "Did not expect more than " << calls_.size() << " tries";
    return default_calls;
  }
  return calls_[current_index_];
}

void MockHttpAgentConfig::ReadData(char* buffer, uint32_t* count) {
  if (current_index_ >= calls_.size()) {
    ADD_FAILURE() << "Reading data for an unexpected call";
    *count = 0;
    return;
  }
  Calls& calls = calls_[current_index_];
  *count =
      std::min(*count, static_cast<uint32_t>(calls.read_data_result.size()));
  memcpy(buffer, calls.read_data_result.c_str(), *count);
  calls.read_data_result = calls.read_data_result.substr(*count);
}

void MockHttpAgentConfig::AddRequestData(const RequestData& request_data) {
  ASSERT_EQ(request_data_.size(), current_index_)
      << "MockHttpAgentConfig does not support creating multiple agents "
      << "without calling Post or Get on each before creating the next one. "
      << "Suggest adding support to MockHttpAgentConfig for that if necessary, "
      << "or updating your code to avoid this.";
  request_data_.push_back(request_data);
}

MockHttpAgentFactory::MockHttpAgentFactory(MockHttpAgentConfig* config)
    : config_(config) {
  DCHECK(config);
}

std::unique_ptr<chrome_cleaner::HttpAgent>
MockHttpAgentFactory::CreateHttpAgent() const {
  // Set the configuration index to the next one (one per HttpAgent).
  if (config_->current_index_ == MockHttpAgentConfig::kInvalidIndex)
    config_->current_index_ = 0;
  else
    ++config_->current_index_;

  return std::make_unique<MockHttpAgent>(config_);
}

}  // namespace chrome_cleaner
