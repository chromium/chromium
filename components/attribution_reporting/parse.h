// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PARSE_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PARSE_H_

#include <string>

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {
struct DefaultConstructTraits;
}  // namespace mojo

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) OsTrigger {
 public:
  // Parses Attribution-Reporting-Register-OS-Trigger header in form of a string
  // and returns a GURL object of the specified header URL.
  // The structured-header item may have parameters, but they are ignored.
  //
  // Returns `absl::nullopt` if `header` is not parsable as a structured-header
  // item, if the item is not a string, if the string is not a valid URL, or if
  // the URL is not potentially trustworthy.
  //
  // Example:
  //
  // "https://x.test/abc"
  //
  // TODO(apaseltiner): Add a fuzzer for this.
  static absl::optional<OsTrigger> Parse(base::StringPiece);

  static absl::optional<OsTrigger> Create(GURL url);

  ~OsTrigger();

  OsTrigger(const OsTrigger&);
  OsTrigger& operator=(const OsTrigger&);

  OsTrigger(OsTrigger&&);
  OsTrigger& operator=(OsTrigger&&);

  const GURL& url() const { return url_; }

 private:
  friend mojo::DefaultConstructTraits;

  // Exposed for Mojo type-mapping.
  OsTrigger();

  explicit OsTrigger(GURL url);

  GURL url_;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) OsSource {
 public:
  // Parses Attribution-Reporting-Register-OS-Source header in form of a string.
  // Requires `header` to contain valid `os-destination` and `web-destination`
  // params.
  //
  // Returns `absl::nullopt` if `header` is not parsable as a structured-header
  // item, if the item is not a string, if the string is not a valid URL, if
  // the URL is not potentially trustworthy, or if either (or both) of the
  // aforementioned params are missing or not a string.
  //
  // Example:
  //
  // "https://x.test/abc"; os-destination="foo";
  // web-destination="https://y.test"
  //
  // TODO(apaseltiner): Add a fuzzer for this.
  static absl::optional<OsSource> Parse(base::StringPiece);

  static absl::optional<OsSource> Create(GURL url,
                                         std::string os_destination,
                                         url::Origin web_destination);

  ~OsSource();

  OsSource(const OsSource&);
  OsSource& operator=(const OsSource&);

  OsSource(OsSource&&);
  OsSource& operator=(OsSource&&);

  const GURL& url() const { return url_; }

  const std::string& os_destination() const { return os_destination_; }

  const url::Origin& web_destination() const { return web_destination_; }

 private:
  friend mojo::DefaultConstructTraits;

  // Exposed for Mojo type-mapping.
  OsSource();

  OsSource(GURL url, std::string os_destination, url::Origin web_destination);

  GURL url_;
  std::string os_destination_;
  url::Origin web_destination_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PARSE_H_
