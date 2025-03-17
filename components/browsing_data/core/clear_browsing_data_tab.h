// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_CLEAR_BROWSING_DATA_TAB_H_
#define COMPONENTS_BROWSING_DATA_CORE_CLEAR_BROWSING_DATA_TAB_H_

namespace browsing_data {

// This enum is used to differentiate CBD preferences from the basic and
// advanced tab and manage their state separately. It is important that all
// preferences and the timeperiod selection have the same type. The default
// value for dialogs without separate tabs is advanced.
// TODO(dullweber): Maybe rename "ADVANCED" to "DEFAULT" because it is used in
//   multiple places without a differentiation between advanced and basic.
enum class ClearBrowsingDataTab { BASIC, ADVANCED, MAX_VALUE = ADVANCED };

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_CLEAR_BROWSING_DATA_TAB_H_
