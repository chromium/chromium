// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_INTENT_CONSTANTS_H_
#define COMPONENTS_ARC_INTENT_HELPER_INTENT_CONSTANTS_H_

namespace arc {

// Well-known Android intent actions, e.g. "android.intent.action.VIEW"
// (corresponding to Android's android.content.Intent.ACTION_VIEW constant).
extern const char kIntentActionMain[];
extern const char kIntentActionView[];
extern const char kIntentActionSend[];
extern const char kIntentActionSendMultiple[];

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_INTENT_CONSTANTS_H_
