// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser.h"

#include <string>

#include "base/strings/stringprintf.h"

namespace update_client {

const char ProtocolParser::Result::kCohort[] = "cohort";
const char ProtocolParser::Result::kCohortHint[] = "cohorthint";
const char ProtocolParser::Result::kCohortName[] = "cohortname";

ProtocolParser::ProtocolParser() = default;
ProtocolParser::~ProtocolParser() = default;

ProtocolParser::Results::Results() = default;
ProtocolParser::Results::Results(const Results& other) = default;
ProtocolParser::Results::~Results() = default;

ProtocolParser::Result::Result() = default;
ProtocolParser::Result::Result(const Result& other) = default;
ProtocolParser::Result::~Result() = default;

ProtocolParser::Result::Manifest::Manifest() = default;
ProtocolParser::Result::Manifest::Manifest(const Manifest& other) = default;
ProtocolParser::Result::Manifest::~Manifest() = default;

ProtocolParser::Result::Manifest::Package::Package() = default;
ProtocolParser::Result::Manifest::Package::Package(const Package& other) =
    default;
ProtocolParser::Result::Manifest::Package::~Package() = default;

ProtocolParser::Result::Data::Data() = default;
ProtocolParser::Result::Data::Data(const Data& other) = default;
ProtocolParser::Result::Data& ProtocolParser::Result::Data::operator=(
    const Data&) = default;
ProtocolParser::Result::Data::Data(const std::string& status,
                                   const std::string& name,
                                   const std::string& install_data_index,
                                   const std::string& text)
    : status(status),
      name(name),
      install_data_index(install_data_index),
      text(text) {}
ProtocolParser::Result::Data::~Data() = default;

void ProtocolParser::ParseError(const char* details, ...) {
  va_list args;
  va_start(args, details);

  if (!errors_.empty()) {
    errors_ += "\r\n";
  }

  base::StringAppendV(&errors_, details, args);
  va_end(args);
}

bool ProtocolParser::Parse(const std::string& response) {
  results_.daystart_elapsed_seconds = kNoDaystart;
  results_.daystart_elapsed_days = kNoDaystart;
  results_.list.clear();
  errors_.clear();

  return DoParse(response, &results_);
}

}  // namespace update_client
