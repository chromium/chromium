// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

namespace update_client {

class ProtocolParser {
 public:
  // The result of parsing one |app| entity in an update check response.
  struct Result {
    struct Manifest {
      struct Package {
        Package();
        Package(const Package& other);
        ~Package();

        std::string fingerprint;

        // Attributes for the full update.
        std::string name;
        std::string hash_sha256;
        int size = 0;

        // Attributes for the differential update.
        std::string namediff;
        std::string hashdiff_sha256;
        int sizediff = 0;
      };

      Manifest();
      Manifest(const Manifest& other);
      ~Manifest();

      std::string version;
      std::string browser_min_version;
      std::vector<Package> packages;
    };

    Result();
    Result(const Result& other);
    ~Result();

    std::string extension_id;

    // The updatecheck response status.
    std::string status;

    // The list of fallback urls, for full and diff updates respectively.
    // These urls are base urls; they don't include the filename.
    std::vector<GURL> crx_urls;
    std::vector<GURL> crx_diffurls;

    Manifest manifest;

    // The server has instructed the client to set its [key] to [value] for each
    // key-value pair in this string.
    std::map<std::string, std::string> cohort_attrs;

    // The following are the only allowed keys in |cohort_attrs|.
    static const char kCohort[];
    static const char kCohortHint[];
    static const char kCohortName[];

    // Contains the run action returned by the server as part of an update
    // check response.
    std::string action_run;
  };

  static const int kNoDaystart = -1;
  struct Results {
    Results();
    Results(const Results& other);
    ~Results();

    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_seconds = kNoDaystart;

    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_days = kNoDaystart;
    std::vector<Result> list;
  };

  static std::unique_ptr<ProtocolParser> Create();

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

  DISALLOW_COPY_AND_ASSIGN(ProtocolParser);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_H_
