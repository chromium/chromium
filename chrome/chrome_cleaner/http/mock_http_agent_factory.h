// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_MOCK_HTTP_AGENT_FACTORY_H_
#define CHROME_CHROME_CLEANER_HTTP_MOCK_HTTP_AGENT_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <string>
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/http/http_status_codes.h"

namespace chrome_cleaner {
class HttpAgent;
}  // namespace chrome_cleaner

namespace chrome_cleaner {

// Stores configuration and results for a mock HttpAgent object created by
// the MockHttpAgentFactory class below.
class MockHttpAgentConfig {
 public:
  // Class used to configure how the various methods should behave when called.
  struct Calls {
    explicit Calls(HttpStatus status);
    Calls(const Calls& other);
    ~Calls();

    Calls& operator=(const Calls& other);

    // Whether a call to Post or Get on the HttpAgent should succeed or not. If
    // it does, an HttpResponse object will be returned and will behave
    // according to the configuration set below. Otherwise, the Post or Get
    // method will return null.
    bool request_succeeds{true};

    // The rest of this struct configures the HttpResponse that will be
    // returned.
    bool get_status_code_succeeds{true};
    HttpStatus get_status_code_result{HttpStatus::kOk};

    bool has_data_succeeds{true};

    // If |read_data_success_sequence| contains one or more values, those will
    // be returned by ReadData in that sequence. When the sequence is empty,
    // |read_data_succeeds_by_default| will be returned for subsequent calls.
    std::vector<bool> read_data_success_sequence;
    bool read_data_succeeds_by_default{true};
    std::string read_data_result;
  };

  // Struct that stores the values passed to Post or Get for validation.
  struct RequestData {
    RequestData();
    RequestData(const RequestData& other);
    ~RequestData();

    RequestData& operator=(const RequestData& other);

    std::wstring host;
    uint16_t port;
    std::wstring path;
    bool secure;
    std::wstring extra_headers;
    std::string body;
  };

  MockHttpAgentConfig();

  MockHttpAgentConfig(const MockHttpAgentConfig&) = delete;
  MockHttpAgentConfig& operator=(const MockHttpAgentConfig&) = delete;

  ~MockHttpAgentConfig();

  // Adds a call configuration. There should be one configuration for each
  // expected call to Post or Get on the HttpAgent (the test will fail
  // otherwise). Returns the index of the new configuration.
  size_t AddCalls(const Calls& calls);

  // Returns the current Calls configuration.
  Calls& GetCurrentCalls();

  // Reads up to |*count| bytes from the |calls_.read_data_result| call
  // configuration string, and returns the data in |buffer|. |count| will be
  // updated with the number of bytes actually read.
  void ReadData(char* buffer, uint32_t* count);

  // Returns the number of calls to Post() that were recorded so far.
  size_t num_request_data() const { return request_data_.size(); }

  // Returns the RequestData for the |index|th call to Post().
  const RequestData& request_data(size_t index) const {
    return request_data_[index];
  }

  // Adds the data passed to a call to Post() or Get(). This should be called
  // only once per Calls configuration. Used by the mock HttpAgent when Post or
  // Get is called.
  void AddRequestData(const RequestData& request_data);

  friend class MockHttpAgentFactory;

 private:
  // Invalid configuration index.
  static const size_t kInvalidIndex = static_cast<size_t>(-1);

  // List of call configuration for every step of the call sequence. When a new
  // HttpAgent is created, the test moves to the next calls configuration.
  std::vector<Calls> calls_;

  // The request data for every call to Post or Get (in sequence).
  std::vector<RequestData> request_data_;

  // The index of the current Calls configuration being used.
  size_t current_index_{kInvalidIndex};
};

// HttpAgent factory that creates mock HttpAgent objects that are controlled by
// a MockHttpAgentConfig object.
class MockHttpAgentFactory : public HttpAgentFactory {
 public:
  explicit MockHttpAgentFactory(MockHttpAgentConfig* config);

  MockHttpAgentFactory(const MockHttpAgentFactory&) = delete;
  MockHttpAgentFactory& operator=(const MockHttpAgentFactory&) = delete;

  // HttpAgentFactory:
  std::unique_ptr<chrome_cleaner::HttpAgent> CreateHttpAgent() const override;

 private:
  MockHttpAgentConfig* config_{nullptr};
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_HTTP_MOCK_HTTP_AGENT_FACTORY_H_
