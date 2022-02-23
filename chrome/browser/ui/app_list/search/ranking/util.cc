// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"

namespace app_list {
namespace {

// The directory within the cryptohome to save ranking state into.
constexpr char kRankerStateDirectory[] = "launcher_ranking/";

}  // namespace

base::FilePath RankerStateDirectory(Profile* profile) {
  return profile->GetPath().AppendASCII(kRankerStateDirectory);
}

}  // namespace app_list
