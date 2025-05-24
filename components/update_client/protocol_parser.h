// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/version.h"
#include "url/gurl.h"

namespace update_client {

class ProtocolParser {
 public:
  // The result of parsing one |app| entity in an update check response.
  struct Data {
    Data();
    Data(const std::string& install_data_index, const std::string& text);
    Data(const Data& other);
    Data& operator=(const Data&);
    ~Data();

    // The `install_data_index` specified by the update check.
    std::string install_data_index;

    // The server data corresponding to the `name`/`install_data_index pair`.
    std::string text;
  };

  struct Operation {
    Operation();
    Operation(const Operation& other);
    Operation& operator=(const Operation&);
    ~Operation();
    std::string type;
    std::string sha256_in;
    std::string sha256_out;
    std::string sha256_previous;
    std::string path;
    std::string arguments;
    int64_t size = 0;
    std::vector<GURL> urls;
  };

  struct Pipeline {
    Pipeline();
    Pipeline(const Pipeline& other);
    Pipeline& operator=(const Pipeline&);
    ~Pipeline();
    std::string pipeline_id;
    std::vector<Operation> operations;
  };

  struct App {
    App();
    App(const App& other);
    App& operator=(const App&);
    ~App();

    std::string app_id;

    // status indicates the outcome of the check. It can be filled from either
    // the app or updatecheck node.
    std::string status;

    // App-specific additions in the updatecheck response, including the
    // mandatory '_' prefix (which prevents collision with formal protocol
    // elements).
    std::map<std::string, std::string> custom_attributes;

    // The server has instructed the client to set its [key] to [value] for each
    // key-value pair in this string.
    std::optional<std::string> cohort;
    std::optional<std::string> cohort_name;
    std::optional<std::string> cohort_hint;

    // Contains the data responses corresponding to the data elements specified
    // in the update request.
    std::vector<Data> data;

    // When an update is available, these fields will be filled.
    base::Version nextversion;
    std::vector<Pipeline> pipelines;
  };

  static constexpr int kNoDaystart = -1;
  struct Results {
    Results();
    Results(const Results& other);
    Results& operator=(const Results&);
    ~Results();

    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_days = kNoDaystart;

    std::vector<App> apps;
  };

  static std::unique_ptr<ProtocolParser> Create();

  ProtocolParser(const ProtocolParser&) = delete;
  ProtocolParser& operator=(const ProtocolParser&) = delete;

  virtual ~ProtocolParser();

  // Parses an update response string into Result data. Returns a bool
  // indicating success or failure. On success, the results are available by
  // calling results(). In case of success, only results corresponding to
  // the update check status |ok| or |noupdate| are included.
  // The details for any failures are available by calling errors().
  bool Parse(const std::string& response);

  const Results& results() const { return results_; }
  const std::string& errors() const { return errors_; }

 protected:
  ProtocolParser();

  // Appends parse error details to |errors_| string.
  void ParseError(const char* details, ...);

 private:
  virtual bool DoParse(const std::string& response, Results* results) = 0;

  Results results_;
  std::string errors_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_H_
