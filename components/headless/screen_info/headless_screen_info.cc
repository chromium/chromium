// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/screen_info/headless_screen_info.h"

#include <optional>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"

using re2::RE2;

namespace headless {

namespace {

// Screen Info parser error messages.
constexpr char kMissingScreenInfo[] = "Missing screen info.";
constexpr char kInvalidScreenInfo[] = "Invalid screen info: ";
constexpr char kUnknownScreenInfoParam[] = "Unknown screen info parameter: ";
constexpr char kInvalidScreenColorDepth[] = "Invalid screen color depth: ";
constexpr char kInvalidScreenIsInternal[] = "Invalid screen is internal: ";
constexpr char kInvalidScreenDevicePixelRatio[] =
    "Invalid screen device pixel ratio: ";
constexpr char kInvalidWorkAreaInset[] = "Invalid work area inset: ";
constexpr char kInvalidRotation[] = "Invalid rotation: ";
constexpr char kNonZeroPrimaryScreenOrigin[] =
    "Primary screen origin can only be at {0,0}";

// Screen Info parameters, keep in sync with window.getScreenDetails() output.
constexpr char kColorDepth[] = "colorDepth";
constexpr char kDevicePixelRatio[] = "devicePixelRatio";
constexpr char kIsInternal[] = "isInternal";
constexpr char kLabel[] = "label";
constexpr char kWorkAreaLeft[] = "workAreaLeft";
constexpr char kWorkAreaRight[] = "workAreaRight";
constexpr char kWorkAreaTop[] = "workAreaTop";
constexpr char kWorkAreaBottom[] = "workAreaBottom";
constexpr char kRotation[] = "rotation";

constexpr int kMinColorDepth = 1;
constexpr float kMinDevicePixelRatio = 0.5f;

void TrimAndUnescape(std::string* str) {
  if (!str->empty() && str->front() == '\'') {
    str->erase(0, 1);
  }

  if (!str->empty() && str->back() == '\'') {
    str->erase(str->size() - 1);
  }

  base::ReplaceSubstringsAfterOffset(str, 0, "\\'", "'");
}

std::optional<bool> GetBooleanParam(std::string_view value) {
  if (value == "1" || value == "true") {
    return true;
  }

  if (value == "0" || value == "false") {
    return false;
  }

  return std::nullopt;
}

// This is the same as display::Display::IsValidRotation(). It is replicated
// here to prevent dependency on //ui/display/ which is undesired because this
// file is intended to be included from there.
bool IsValidRotation(int degrees) {
  return degrees == 0 || degrees == 90 || degrees == 180 || degrees == 270;
}

// Parse screen info parameter key value pair returning an error message or
// an empty string if there is no error.
std::string ParseScreenInfoParameter(std::string_view key,
                                     std::string_view value,
                                     HeadlessScreenInfo* screen_info) {
  // colorDepth=24
  if (key == kColorDepth) {
    int color_depth;
    if (!base::StringToInt(value, &color_depth) ||
        color_depth < kMinColorDepth) {
      return kInvalidScreenColorDepth + std::string(value);
    }
    screen_info->color_depth = color_depth;
    return {};
  }

  // devicePixelRatio=1.0
  if (key == kDevicePixelRatio) {
    double device_pixel_ratio;
    if (!base::StringToDouble(value, &device_pixel_ratio) ||
        device_pixel_ratio < kMinDevicePixelRatio) {
      return kInvalidScreenDevicePixelRatio + std::string(value);
    }
    screen_info->device_pixel_ratio = static_cast<float>(device_pixel_ratio);
    return {};
  }

  // isInternal=0|1|false|true
  if (key == kIsInternal) {
    std::optional<bool> is_internal_opt = GetBooleanParam(value);
    if (!is_internal_opt) {
      return kInvalidScreenIsInternal + std::string(value);
    }

    screen_info->is_internal = is_internal_opt.value();
    return {};
  }

  // label='primary screen'
  if (key == kLabel) {
    screen_info->label = value;
    return {};
  }

  // workAreaLeft=NNN
  if (key == kWorkAreaLeft) {
    int work_area_left;
    if (!base::StringToInt(value, &work_area_left) || work_area_left < 0) {
      return kInvalidWorkAreaInset + std::string(value);
    }

    screen_info->work_area_insets.set_left(work_area_left);
    return {};
  }

  // workAreaRight=NNN
  if (key == kWorkAreaRight) {
    int work_area_right;
    if (!base::StringToInt(value, &work_area_right) || work_area_right < 0) {
      return kInvalidWorkAreaInset + std::string(value);
    }

    screen_info->work_area_insets.set_right(work_area_right);
    return {};
  }

  // workAreaTop=NNN
  if (key == kWorkAreaTop) {
    int work_area_top;
    if (!base::StringToInt(value, &work_area_top) || work_area_top < 0) {
      return kInvalidWorkAreaInset + std::string(value);
    }

    screen_info->work_area_insets.set_top(work_area_top);
    return {};
  }

  // workAreaBottom=NNN
  if (key == kWorkAreaBottom) {
    int work_area_bottom;
    if (!base::StringToInt(value, &work_area_bottom) || work_area_bottom < 0) {
      return kInvalidWorkAreaInset + std::string(value);
    }

    screen_info->work_area_insets.set_bottom(work_area_bottom);
    return {};
  }

  // rotation=0|90|180|270
  if (key == kRotation) {
    int rotation;
    if (!base::StringToInt(value, &rotation) || !IsValidRotation(rotation)) {
      return kInvalidRotation + std::string(value);
    }

    screen_info->rotation = rotation;
    return {};
  }

  return kUnknownScreenInfoParam + std::string(key);
}

// Parse a single screen info specification in the format of
// [X,Y] WxH [key=value ...] returning an error message or
// an empty string if there is no error.
std::string ParseOneScreenInfo(std::string_view screen_info,
                               std::vector<HeadlessScreenInfo>& result) {
  HeadlessScreenInfo new_screen_info;

  // Scan in the screen origin if any, matching any leading space, then
  // optional '-' followed by one or more digits, followed by comma and
  // another optional '-' followed by one or more digits.
  int x, y;
  if (RE2::Consume(&screen_info, R"(\s*(-?\d+),(-?\d+)\s*)", &x, &y)) {
    new_screen_info.bounds.set_origin({x, y});
  } else if (!result.empty()) {
    // If no origin is given for a secondary screen shift it to the
    // right of the previous screen so that they don't overlap.
    const HeadlessScreenInfo& prev_screen = result.back();
    new_screen_info.bounds.set_origin(
        {prev_screen.bounds.right(), prev_screen.bounds.y()});
  }

  // Scan in the screen size if any, matching any leading white space followed
  // by one or more digits, followed by an 'x' followed by one or more digits.
  int width, height;
  if (RE2::Consume(&screen_info, R"(\s*(\d+)x(\d+)\s*)", &width, &height)) {
    new_screen_info.bounds.set_size({width, height});
  }

  // Scan in the screen info parameters key value pairs.
  while (!screen_info.empty()) {
    std::string key, value;
    if (RE2::Consume(&screen_info,
                     R"(\s*([^= ]*)=('(?:\\.|[^'\\]+)*'|[^ ']*)\s*)", &key,
                     &value)) {
      TrimAndUnescape(&key);
      TrimAndUnescape(&value);
      std::string error =
          ParseScreenInfoParameter(key, value, &new_screen_info);
      if (!error.empty()) {
        return error;
      }
    } else {
      std::string leftover_text;
      base::TrimWhitespaceASCII(screen_info, base::TRIM_ALL, &leftover_text);
      if (!leftover_text.empty()) {
        return kInvalidScreenInfo + leftover_text;
      }
      break;
    }
  }

  if (result.empty() && !new_screen_info.bounds.origin().IsOrigin()) {
    return kNonZeroPrimaryScreenOrigin;
  }

  result.push_back(new_screen_info);

  return {};
}

}  // namespace

bool HeadlessScreenInfo::operator==(const HeadlessScreenInfo& other) const =
    default;

// static
base::expected<std::vector<HeadlessScreenInfo>, std::string>
HeadlessScreenInfo::FromString(std::string_view screen_info) {
  std::vector<HeadlessScreenInfo> result;

  // Match leading space before '{', grab everything until the '}', followed
  // by an optional ',' or spaces.
  RE2 re(R"(\s*{([^}]*)}\s*,?\s*)");
  while (!screen_info.empty()) {
    std::string one_screen_info;
    if (RE2::Consume(&screen_info, re, &one_screen_info)) {
      std::string error = ParseOneScreenInfo(one_screen_info, result);
      if (!error.empty()) {
        return base::unexpected(error);
      }
    } else if (!screen_info.empty()) {
      return base::unexpected(kInvalidScreenInfo + std::string(screen_info));
    }
  }

  if (result.empty()) {
    return base::unexpected(kMissingScreenInfo);
  }

  return base::ok(result);
}

std::string HeadlessScreenInfo::ToString() const {
  return base::StringPrintf(
      "%s color_depth=%d device_pixel_ratio=%g is_internal=%d label='%s' "
      "workarea TLBR={%d,%d,%d,%d} rotation=%d",
      bounds.ToString().c_str(), color_depth, device_pixel_ratio, is_internal,
      label.c_str(), work_area_insets.top(), work_area_insets.left(),
      work_area_insets.bottom(), work_area_insets.right(), rotation);
}

}  // namespace headless
