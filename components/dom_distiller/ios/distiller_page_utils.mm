// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/dom_distiller/ios/distiller_page_utils.h"

#import "base/check_op.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "third_party/dom_distiller_js/dom_distiller.pb.h"
#import "third_party/dom_distiller_js/dom_distiller_json_converter.h"

namespace dom_distiller {
namespace {

// This is duplicated here from ios/web/js_messaging/web_view_js_utils.mm in
// order to handle numbers. The dom distiller proto expects integers and the
// generated JSON deserializer does not accept doubles in the place of ints.
// However WKWebView only returns "numbers." However, here the proto expects
// integers and doubles, which is done by checking if the number has a fraction
// or not; since this is a hacky method it's isolated to this file so as to
// limit the risk of broken JS calls.
int const kMaximumParsingRecursionDepth = 6;

// Returns a clone of `value` where double values are converted to integers if
// the numbers has no fraction. `value` is only processed up to `max_depth`.
// TODO(crbug.com/404833789): Remove this workaround when the DOMDistillerResult
// JSON converter no longer requires double to int conversion.
base::Value ConvertedResultFromScriptResult(const base::Value* value,
                                            int max_depth) {
  base::Value result;
  if (!value || value->is_none()) {
    DCHECK_EQ(result.type(), base::Value::Type::NONE);
    return result;
  }

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  if (value->is_string()) {
    result = base::Value(value->GetString());
    DCHECK_EQ(result.type(), base::Value::Type::STRING);
  } else if (value->is_double()) {
    // Different implementation is here.
    double double_value = value->GetDouble();
    int int_value = round(double_value);
    if (double_value == int_value) {
      result = base::Value(int_value);
      DCHECK_EQ(result.type(), base::Value::Type::INTEGER);
    } else {
      result = base::Value(double_value);
      DCHECK_EQ(result.type(), base::Value::Type::DOUBLE);
    }
    // End of different implementation.
  } else if (value->is_int()) {
    result = base::Value(value->GetInt());
    DCHECK_EQ(result.type(), base::Value::Type::INTEGER);
  } else if (value->is_bool()) {
    result = base::Value(value->GetBool());
    DCHECK_EQ(result.type(), base::Value::Type::BOOLEAN);
  } else if (value->is_dict()) {
    base::Value::Dict dictionary;
    for (const auto kv : value->GetDict()) {
      base::Value item_value =
          ConvertedResultFromScriptResult(&kv.second, max_depth - 1);

      if (item_value.type() == base::Value::Type::NONE) {
        return result;
      }
      dictionary.SetByDottedPath(kv.first, std::move(item_value));
    }
    result = base::Value(std::move(dictionary));
    DCHECK_EQ(result.type(), base::Value::Type::DICT);

  } else if (value->is_list()) {
    base::Value::List list;
    for (const base::Value& list_item : value->GetList()) {
      base::Value converted_item =
          ConvertedResultFromScriptResult(&list_item, max_depth - 1);
      if (converted_item.type() == base::Value::Type::NONE) {
        return result;
      }

      list.Append(std::move(converted_item));
    }
    result = base::Value(std::move(list));
    DCHECK_EQ(result.type(), base::Value::Type::LIST);
  } else {
    NOTREACHED();  // Convert other types as needed.
  }
  return result;
}

}  // namespace

base::Value ParseValueFromScriptResult(const base::Value* value) {
  return ConvertedResultFromScriptResult(value, kMaximumParsingRecursionDepth);
}

}  // namespace dom_distiller
