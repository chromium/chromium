// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_IPP_L10N_H_
#define COMPONENTS_PRINTING_BROWSER_IPP_L10N_H_

#include <map>

#include "base/strings/string_piece.h"

const std::map<base::StringPiece, int>& CapabilityLocalizationMap();

#endif  // COMPONENTS_PRINTING_BROWSER_IPP_L10N_H_
