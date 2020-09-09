// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/share_target.h"

#include <tuple>

#include "base/strings/string_util.h"

namespace apps {

ShareTarget::Files::Files() = default;

ShareTarget::Files::Files(const ShareTarget::Files&) = default;

ShareTarget::Files::Files(ShareTarget::Files&&) = default;

ShareTarget::Files& ShareTarget::Files::operator=(const ShareTarget::Files&) =
    default;

ShareTarget::Files& ShareTarget::Files::operator=(ShareTarget::Files&&) =
    default;

ShareTarget::Files::~Files() = default;

ShareTarget::Params::Params() = default;

ShareTarget::Params::Params(const ShareTarget::Params&) = default;

ShareTarget::Params::Params(ShareTarget::Params&&) = default;

ShareTarget::Params& ShareTarget::Params::operator=(
    const ShareTarget::Params&) = default;

ShareTarget::Params& ShareTarget::Params::operator=(ShareTarget::Params&&) =
    default;

ShareTarget::Params::~Params() = default;

ShareTarget::ShareTarget() = default;

ShareTarget::ShareTarget(const ShareTarget&) = default;

ShareTarget::ShareTarget(ShareTarget&&) = default;

ShareTarget& ShareTarget::operator=(const ShareTarget&) = default;

ShareTarget& ShareTarget::operator=(ShareTarget&&) = default;

ShareTarget::~ShareTarget() = default;

// static
const char* ShareTarget::MethodToString(ShareTarget::Method method) {
  switch (method) {
    case Method::kGet:
      return "GET";
    case Method::kPost:
      return "POST";
  }
}

// static
const char* ShareTarget::EnctypeToString(ShareTarget::Enctype enctype) {
  switch (enctype) {
    case Enctype::kFormUrlEncoded:
      return "application/x-www-form-urlencoded";
    case Enctype::kMultipartFormData:
      return "multipart/form-data";
  }
}

std::ostream& operator<<(std::ostream& out, const ShareTarget& share_target) {
  out << "action: " << share_target.action << std::endl;
  out << "method: " << ShareTarget::MethodToString(share_target.method)
      << std::endl;
  out << "enctype: " << ShareTarget::EnctypeToString(share_target.enctype)
      << std::endl;
  out << share_target.params;
  return out;
}

std::ostream& operator<<(std::ostream& out, const ShareTarget::Params& params) {
  out << "title: " << params.title << std::endl;
  out << "text: " << params.text << std::endl;
  out << "url: " << params.url << std::endl;
  for (const auto& files_entry : params.files)
    out << files_entry;
  return out;
}

std::ostream& operator<<(std::ostream& out, const ShareTarget::Files& files) {
  out << "name: " << files.name << std::endl;
  for (const auto& accept_entry : files.accept)
    out << "accept: " << accept_entry << std::endl;
  return out;
}

bool operator==(const ShareTarget& share_target1,
                const ShareTarget& share_target2) {
  return std::tie(share_target1.action, share_target1.method,
                  share_target1.enctype, share_target1.params) ==
         std::tie(share_target2.action, share_target2.method,
                  share_target2.enctype, share_target2.params);
}

bool operator==(const ShareTarget::Params& params1,
                const ShareTarget::Params& params2) {
  return std::tie(params1.title, params1.text, params1.url, params1.files) ==
         std::tie(params2.title, params2.text, params2.url, params2.files);
}

bool operator==(const ShareTarget::Files& files1,
                const ShareTarget::Files& files2) {
  return std::tie(files1.name, files1.accept) ==
         std::tie(files2.name, files2.accept);
}

bool operator!=(const ShareTarget& share_target1,
                const ShareTarget& share_target2) {
  return !(share_target1 == share_target2);
}

bool operator!=(const ShareTarget::Params& params1,
                const ShareTarget::Params& params2) {
  return !(params1 == params2);
}

bool operator!=(const ShareTarget::Files& files1,
                const ShareTarget::Files& files2) {
  return !(files1 == files2);
}

}  // namespace apps
