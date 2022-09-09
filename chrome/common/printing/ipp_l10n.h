// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_IPP_L10N_H_
#define CHROME_COMMON_PRINTING_IPP_L10N_H_

#include <map>

#include "base/strings/string_piece.h"

const std::map<base::StringPiece, int>& CapabilityLocalizationMap();

#endif  // CHROME_COMMON_PRINTING_IPP_L10N_H_
