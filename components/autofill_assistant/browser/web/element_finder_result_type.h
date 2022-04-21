// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_RESULT_TYPE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_RESULT_TYPE_H_

namespace autofill_assistant {

enum class ElementFinderResultType {
  // ElementFinderResult.object_id contains the object ID of the single node
  // that matched.
  // If there are no matches, status is ELEMENT_RESOLUTION_FAILED. If there
  // are more than one matches, status is TOO_MANY_ELEMENTS.
  kExactlyOneMatch = 0,

  // ElementFinderResult.object_id contains the object ID of one of the nodes
  // that matched.
  // If there are no matches, status is ELEMENT_RESOLUTION_FAILED.
  kAnyMatch,

  // ElementFinderResult.object_id contains the object ID of an array
  // containing all the
  // nodes
  // that matched. If there are no matches, status is
  // ELEMENT_RESOLUTION_FAILED.
  kMatchArray,
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_RESULT_H_
