// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_INFO_BOX_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_INFO_BOX_H_

#include <map>
#include <string>

#include "base/values.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class InfoBox {
 public:
  InfoBox() = default;
  InfoBox(const ShowInfoBoxProto& proto);

  const ShowInfoBoxProto& proto() const { return proto_; }

  // Returns a dictionary describing the current execution context, which
  // is intended to be serialized as JSON string. The execution context is
  // useful when analyzing feedback forms and for debugging in general.
  base::Value GetDebugContext() const;

 private:
  const InfoBoxProto& info_box() const { return proto_.info_box(); }
  ShowInfoBoxProto proto_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_INFO_BOX_H_
