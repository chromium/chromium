// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_ITRANS_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_ITRANS_H_

namespace ta_itrans {

// The id of this IME/keyboard.
extern const char* kId;

// The transform rules definition. The string items in the even indexes are
// the regular expressions represent what needs to be transformed, and the
// ones in the odd indexes represent it can transform to what.
extern const char* kTransforms[];

// The length of the transform rules.
extern const unsigned int kTransformsLen;

// The history prune regexp.
extern const char* kHistoryPrune;

}  // namespace ta_itrans

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_TA_ITRANS_H_
