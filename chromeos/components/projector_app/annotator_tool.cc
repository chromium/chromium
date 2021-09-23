// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/annotator_tool.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromeos {

namespace {

const char kToolColor[] = "color";
const char kToolSize[] = "size";
const char kToolType[] = "type";

// For example, SK_ColorGREEN -> "FF00FF00".
std::string ConvertColorToHexString(SkColor color) {
  uint8_t alpha = SkColorGetA(color);
  uint8_t red = SkColorGetR(color);
  uint8_t green = SkColorGetG(color);
  uint8_t blue = SkColorGetB(color);
  uint8_t bytes[] = {alpha, red, green, blue};
  return base::HexEncode(bytes);
}

// For example, "FF000000" -> SK_ColorBLACK.
// Returns red if conversion fails.
SkColor ConvertHexStringToColor(const std::string& hex) {
  uint32_t color;
  const bool success = base::HexStringToUInt(hex, &color);
  DCHECK(success);
  return success ? color : SK_ColorRED;
}

}  // namespace

// static
AnnotatorTool AnnotatorTool::FromValue(const base::Value& value) {
  DCHECK(value.is_dict());
  DCHECK(value.FindKey(kToolColor));
  DCHECK(value.FindKey(kToolColor)->is_string());
  DCHECK(value.FindKey(kToolSize));
  DCHECK(value.FindKey(kToolSize)->is_int());
  DCHECK(value.FindKey(kToolType));
  DCHECK(value.FindKey(kToolType)->is_int());
  AnnotatorTool t;
  t.color = ConvertHexStringToColor(*(value.FindStringPath(kToolColor)));
  t.size = *(value.FindIntPath(kToolSize));
  t.type = static_cast<AnnotatorToolType>(*(value.FindIntPath(kToolType)));
  return t;
}

base::Value AnnotatorTool::ToValue() const {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetKey(kToolColor, base::Value(ConvertColorToHexString(color)));
  val.SetKey(kToolSize, base::Value(size));
  val.SetKey(kToolType, base::Value(static_cast<int>(type)));
  return val;
}

bool AnnotatorTool::operator==(const AnnotatorTool& rhs) const {
  return rhs.color == color && rhs.size == size && rhs.type == type;
}

}  // namespace chromeos
