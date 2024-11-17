// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/label_source_util.h"

#include <string>

namespace autofill {

std::string LabelSourceToString(FormFieldData::LabelSource label_source) {
  switch (label_source) {
    case FormFieldData::LabelSource::kUnknown:
      return "NoLabel";
    case FormFieldData::LabelSource::kLabelTag:
      return "LabelTag";
    case FormFieldData::LabelSource::kPTag:
      return "PTag";
    case FormFieldData::LabelSource::kDivTable:
      return "DivTable";
    case FormFieldData::LabelSource::kTdTag:
      return "TdTag";
    case FormFieldData::LabelSource::kDdTag:
      return "DdTag";
    case FormFieldData::LabelSource::kLiTag:
      return "LiTag";
    case FormFieldData::LabelSource::kPlaceHolder:
      return "Placeholder";
    case FormFieldData::LabelSource::kAriaLabel:
      return "AriaLabel";
    case FormFieldData::LabelSource::kCombined:
      return "Combined";
    case FormFieldData::LabelSource::kValue:
      return "Value";
    case FormFieldData::LabelSource::kForId:
      return "ForId";
    case FormFieldData::LabelSource::kForName:
      return "ForName";
    case FormFieldData::LabelSource::kForShadowHostId:
      return "ForShadowHostId";
    case FormFieldData::LabelSource::kForShadowHostName:
      return "ForShadowHostName";
    case FormFieldData::LabelSource::kOverlayingLabel:
      return "OverlayingLabel";
    case FormFieldData::LabelSource::kDefaultSelectText:
      return "DefaultSelectText";
  }
}

}  // namespace autofill
