// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHARE_TARGET_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHARE_TARGET_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/values.h"
#include "url/gurl.h"

namespace apps {

// https://w3c.github.io/web-share-target/level-2/#sharetarget-and-its-members
// https://w3c.github.io/web-share-target/level-2/#sharetargetfiles-and-its-members
// https://w3c.github.io/web-share-target/level-2/#sharetargetparams-and-its-members

struct ShareTarget {
  enum class Method {
    kGet,
    kPost,
  };

  enum class Enctype {
    kFormUrlEncoded,
    kMultipartFormData,
  };

  struct Files {
    Files();
    Files(const Files&);
    Files(Files&&);
    Files& operator=(const Files&);
    Files& operator=(Files&&);
    ~Files();

    friend bool operator==(const Files&, const Files&) = default;

    base::Value AsDebugValue() const;

    std::string name;
    std::vector<std::string> accept;
  };

  struct Params {
    Params();
    Params(const Params&);
    Params(Params&&);
    Params& operator=(const Params&);
    Params& operator=(Params&&);
    ~Params();

    friend bool operator==(const Params&, const Params&) = default;

    base::Value AsDebugValue() const;

    std::string title;
    std::string text;
    std::string url;
    std::vector<Files> files;
  };

  ShareTarget();
  ShareTarget(const ShareTarget&);
  ShareTarget(ShareTarget&&);
  ShareTarget& operator=(const ShareTarget&);
  ShareTarget& operator=(ShareTarget&&);
  ~ShareTarget();

  friend bool operator==(const ShareTarget&, const ShareTarget&) = default;

  static const char* MethodToString(Method);
  static const char* EnctypeToString(Enctype);

  base::Value AsDebugValue() const;

  GURL action;

  Method method = Method::kGet;

  Enctype enctype = Enctype::kFormUrlEncoded;

  Params params;
};
}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_SHARE_TARGET_H_
