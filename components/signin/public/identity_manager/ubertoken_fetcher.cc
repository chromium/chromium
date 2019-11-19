// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/ubertoken_fetcher.h"

namespace signin {

// While defining providing an implementation to pure virtual methods is rarely
// useful, one must define a pure virtual destructor. This is because the
// destructor of a base class is always called when a derived object is
// destroyed. Failing to define it will cause a link error.
UbertokenFetcher::~UbertokenFetcher() {}

}  // namespace signin
