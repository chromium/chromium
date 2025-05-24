// Copyright 2020 The Chromium Authors
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

base::Value ShareTarget::Files::AsDebugValue() const {
  base::Value::Dict root;
  root.Set("name", name);
  base::Value::List& accept_json = *root.EnsureList("accept");
  for (const std::string& entry : accept)
    accept_json.Append(entry);
  return base::Value(std::move(root));
}

ShareTarget::Params::Params() = default;

ShareTarget::Params::Params(const ShareTarget::Params&) = default;

ShareTarget::Params::Params(ShareTarget::Params&&) = default;

ShareTarget::Params& ShareTarget::Params::operator=(
    const ShareTarget::Params&) = default;

ShareTarget::Params& ShareTarget::Params::operator=(ShareTarget::Params&&) =
    default;

ShareTarget::Params::~Params() = default;

base::Value ShareTarget::Params::AsDebugValue() const {
  base::Value::Dict root;
  root.Set("title", title);
  root.Set("text", text);
  root.Set("url", url);
  base::Value::List& files_json = *root.EnsureList("files");
  for (const auto& files_entry : files)
    files_json.Append(files_entry.AsDebugValue());
  return base::Value(std::move(root));
}

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

base::Value ShareTarget::AsDebugValue() const {
  base::Value::Dict root;
  root.Set("action", action.spec());
  root.Set("method", ShareTarget::MethodToString(method));
  root.Set("enctype", ShareTarget::EnctypeToString(enctype));
  root.Set("params", params.AsDebugValue());
  return base::Value(std::move(root));
}

}  // namespace apps
