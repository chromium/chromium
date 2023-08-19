// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/build_info.h"

#include <string>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "chromecast/base/version.h"
namespace chromecast {
namespace {

constexpr char kEngVariant[] = "eng";
constexpr char kUserVariant[] = "user";

}  // namespace

const std::string VersionToCrashString(const std::string& cast_build_revision) {
  // Incremental number for eng+user builds is too long for Crash server
  // so cap it to "eng" or "user".
  for (std::string infix : {kEngVariant, kUserVariant}) {
    size_t index = cast_build_revision.find(infix);
    if (index != std::string::npos) {
      return cast_build_revision.substr(
          0, index + infix.size());  // Truncate after ".eng" / ".user".
    }
  }
  return cast_build_revision;
}

const std::string GetVersionString() {
  return VersionToCrashString(CAST_BUILD_REVISION);
}

const std::string GetVersionString(const std::string& cast_release_number,
                                   const std::string& cast_incremental_number) {
  if (cast_release_number.empty() || cast_incremental_number.empty()) {
    return VersionToCrashString(CAST_BUILD_REVISION);
  }
  return VersionToCrashString(
      base::JoinString({cast_release_number, cast_incremental_number}, "."));
}

const std::string VersionToVariant(const std::string& cast_build_revision) {
  for (const std::string& variant : {kEngVariant, kUserVariant}) {
    if (base::Contains(cast_build_revision, variant)) {
      return variant;
    }
  }
  return CAST_IS_DEBUG_BUILD() ? kEngVariant : kUserVariant;
}

const std::string GetBuildVariant() {
  return VersionToVariant(CAST_BUILD_REVISION);
}

}  // namespace chromecast
