// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_BREAKAGE_EXCEPTION_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_BREAKAGE_EXCEPTION_H_

#include <string>

#include "url/gurl.h"

class PrefService;

namespace fingerprinting_protection_filter {

// Computes the registrable domain (eTLD+1), which is the key used for
// storing/querying FP breakage exceptions. We use eTLD+1 because it's assumed
// that, if one page is broken by FP, every page on the same site may also be
// broken by FP. If eTLD+1 cannot be computed (the URL is invalid), the empty
// string is returned.
// Tested implicitly through `AddBreakageException` and `HasBreakageException`
// tests.
std::string GetEtldPlusOne(const GURL& url);

// Ensures an exception is recorded for the given URL in a profile-level pref,
// adding it if necessary. If the URL is invalid, nothing is done and returns
// `false`. Otherwise, returns `true`.
bool AddBreakageException(const GURL& url, PrefService& pref_service);

// Returns true if the given URL has an exception recorded in a profile-level
// pref. If the URL is invalid, returns `false`.
bool HasBreakageException(const GURL& url, const PrefService& pref_service);

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_BREAKAGE_EXCEPTION_H_
