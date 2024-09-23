// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_ERROR_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_ERROR_H_

namespace ash::babelorca {

enum class TachyonRequestError {
  kHttpError,
  kNetworkError,
  kInternalError,
  kAuthError
};

}

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_REQUEST_ERROR_H_
