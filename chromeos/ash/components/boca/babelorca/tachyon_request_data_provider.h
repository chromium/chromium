// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_DATA_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_DATA_PROVIDER_H_

#include <string>

namespace ash::babelorca {

// Provider for tachyon requests common data.
class TachyonRequestDataProvider {
 public:
  TachyonRequestDataProvider(const TachyonRequestDataProvider&) = delete;
  TachyonRequestDataProvider& operator=(const TachyonRequestDataProvider&) =
      delete;

  virtual ~TachyonRequestDataProvider() = default;

  virtual std::string session_id() = 0;
  virtual std::string tachyon_token() = 0;
  virtual std::string group_id() = 0;
  virtual std::string sender_email() = 0;

 protected:
  TachyonRequestDataProvider() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_DATA_PROVIDER_H_
