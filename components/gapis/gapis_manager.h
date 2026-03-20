// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAPIS_GAPIS_MANAGER_H_
#define COMPONENTS_GAPIS_GAPIS_MANAGER_H_

#include <string>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"

namespace gapis {

// A singleton class that manages obtained app tokens.
class GapisManager {
 public:
  GapisManager(const GapisManager&) = delete;
  GapisManager& operator=(const GapisManager&) = delete;

  ~GapisManager();

  static GapisManager* GetInstance();

  void SetAppToken(const std::string& app_token);
  std::string GetAppToken() const;

 private:
  GapisManager();
  friend class base::NoDestructor<GapisManager>;

  SEQUENCE_CHECKER(sequence_checker_);

  std::string app_token_;
};

}  // namespace gapis

#endif  // COMPONENTS_GAPIS_GAPIS_MANAGER_H_
