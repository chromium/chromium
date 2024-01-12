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

        // |fingerprint| is optional. It identifies the package, preferably
        // with a modified sha256 hash of the package in hex format.
        std::string fingerprint;

        // Attributes for the full update.
        std::string name;
        std::string hash_sha256;
        int64_t size = 0;

        // Attributes for the differential update.
        std::string namediff;
        std::string hashdiff_sha256;
        int64_t sizediff = 0;
      };

      Manifest();
      Manifest(const Manifest& other);
      ~Manifest();

      std::string version;
      std::string browser_min_version;
      std::vector<Package> packages;

      // A path within the CRX archive to an executable to run as part of the
      // update. The executable is typically an application installer.
      std::string run;

      // Command-line arguments for the binary specified by |run|.
      std::string arguments;
    };

    // Optional `data` element.
    struct Data {
      Data();
      Data(const Data& other);
      Data& operator=(const Data&);
      Data(const std::string& status,
           const std::string& name,
           const std::string& install_data_index,
           const std::string& text);
      ~Data();

      // "ok" if the server could successfully find a match for the `data`
      // element in the update request, or an error string otherwise.
      std::string status;

      // If `status` is "ok", this contains the `name`. The only value supported
      // for `name` by the server currently is "install".
      std::string name;

      // The `install_data_index` specified by the update check.
      std::string install_data_index;

      // The server data corresponding to the `name`/`install_data_index pair`.
      std::string text;
    };

    Result();
    Result(const Result& other);
    ~Result();

    std::string extension_id;

    // The updatecheck response status.
    std::string status;

    // App-specific additions in the updatecheck response, including the
    // mandatory '_' prefix (which prevents collision with formal protocol
    // elements).
    std::map<std::string, std::string> custom_attributes;

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
    // check response. This indicates the need to trigger the execution of
    // something bound to a component which is already installed.
    std::string action_run;

    // Contains the data responses corresponding to the data elements specified
    // in the update request.
    std::vector<Data> data;
  };

  struct SystemRequirements {
    std::string platform;  // For example, "win".

    // Expected host processor architecture that the app is compatible with.
    // `arch` can be a single entry, or multiple entries separated with `,`.
    // Entries prefixed with a `-` (negative entries) indicate non-compatible
    // hosts.
    //
    // Examples:
    // * `arch` == "x86".
    // * `arch` == "x64".
    // * `arch` == "x86,x64,-arm64": the app will fail installation if the
    // underlying host is arm64.
    std::string arch;

    std::string min_os_version;  // major.minor.
  };

  static constexpr int kNoDaystart = -1;
  struct Results {
    Results();
    Results(const Results& other);
    ~Results();

    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_seconds = kNoDaystart;

    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_days = kNoDaystart;

    SystemRequirements system_requirements;
    std::vector<Result> list;
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
