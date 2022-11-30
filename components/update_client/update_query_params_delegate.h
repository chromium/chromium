// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_DELEGATE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_DELEGATE_H_

#include <string>

namespace update_client {

// Embedders can specify an UpdateQueryParamsDelegate to provide additional
// custom parameters. If not specified (Set is never called), no additional
// parameters are added.
class UpdateQueryParamsDelegate {
 public:
  UpdateQueryParamsDelegate();

  UpdateQueryParamsDelegate(const UpdateQueryParamsDelegate&) = delete;
  UpdateQueryParamsDelegate& operator=(const UpdateQueryParamsDelegate&) =
      delete;

  virtual ~UpdateQueryParamsDelegate();

  // Returns additional parameters, if any. If there are any parameters, the
  // string should begin with a & character.
  virtual std::string GetExtraParams() = 0;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_DELEGATE_H_
