// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_util.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "third_party/webdriver/atoms.h"

namespace {

const char kElementKey[] = "ELEMENT";
const char kElementKeyW3C[] = "element-6066-11e4-a52e-4f735466cecf";

bool ParseFromValue(base::Value* value, WebPoint* point) {
  base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;
  double x = 0;
  double y = 0;
  if (!dict_value->GetDouble("x", &x) ||
      !dict_value->GetDouble("y", &y))
    return false;
  point->x = static_cast<int>(x);
  point->y = static_cast<int>(y);
  return true;
}

bool ParseFromValue(base::Value* value, WebSize* size) {
  base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;
  double width = 0;
  double height = 0;
  if (!dict_value->GetDouble("width", &width) ||
      !dict_value->GetDouble("height", &height))
    return false;
  size->width = static_cast<int>(width);
  size->height = static_cast<int>(height);
  return true;
}

bool ParseFromValue(base::Value* value, WebRect* rect) {
  base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
  if (!dict_value->GetDouble("left", &x) ||
      !dict_value->GetDouble("top", &y) ||
      !dict_value->GetDouble("width", &width) ||
      !dict_value->GetDouble("height", &height))
    return false;
  rect->origin.x = static_cast<int>(x);
  rect->origin.y = static_cast<int>(y);
  rect->size.width = static_cast<int>(width);
  rect->size.height = static_cast<int>(height);
  return true;
}

std::unique_ptr<base::DictionaryValue> CreateValueFrom(const WebRect& rect) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("left", rect.X());
  dict->SetInteger("top", rect.Y());
  dict->SetInteger("width", rect.Width());
  dict->SetInteger("height", rect.Height());
  return dict;
}

Status CallAtomsJs(const std::string& frame,
                   WebView* web_view,
                   const char* const* atom_function,
                   const base::ListValue& args,
                   std::unique_ptr<base::Value>* result) {
  return web_view->CallFunction(
      frame, webdriver::atoms::asString(atom_function), args, result);
}

Status VerifyElementClickable(
    const std::string& frame,
    WebView* web_view,
    const std::string& element_id,
    const WebPoint& location) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.Append(CreateValueFrom(location));
  std::unique_ptr<base::Value> result;
  status = CallAtomsJs(
      frame, web_view, webdriver::atoms::IS_ELEMENT_CLICKABLE,
      args, &result);
  if (status.IsError())
    return status;
  base::DictionaryValue* dict;
  bool is_clickable = false;
  if (!result->GetAsDictionary(&dict) ||
      !dict->GetBoolean("clickable", &is_clickable)) {
    return Status(kUnknownError,
                  "failed to parse value of IS_ELEMENT_CLICKABLE");
  }

  if (!is_clickable) {
    std::string message;
    if (!dict->GetString("message", &message))
      message = "element click intercepted";
    return Status(kElementClickIntercepted, message);
  }
  return Status(kOk);
}

Status ScrollElementRegionIntoViewHelper(
    const std::string& frame,
    WebView* web_view,
    const std::string& element_id,
    const WebRect& region,
    bool center,
    const std::string& clickable_element_id,
    WebPoint* location) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  WebPoint tmp_location = *location;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.AppendBoolean(center);
  args.Append(CreateValueFrom(region));
  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      frame, webdriver::atoms::asString(webdriver::atoms::GET_LOCATION_IN_VIEW),
      args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), &tmp_location)) {
    return Status(kUnknownError,
                  "failed to parse value of GET_LOCATION_IN_VIEW");
  }
  if (!clickable_element_id.empty()) {
    WebPoint middle = tmp_location;
    middle.Offset(region.Width() / 2, region.Height() / 2);
    status = VerifyElementClickable(
        frame, web_view, clickable_element_id, middle);
    if (status.code() == kElementClickIntercepted) {
      // Clicking at the target location isn't reaching the target element.
      // One possible cause is a scroll event handler has shifted the element.
      // Try again to get the updated location of the target element.
      status = web_view->CallFunction(
          frame,
          webdriver::atoms::asString(webdriver::atoms::GET_LOCATION_IN_VIEW),
          args, &result);
      if (status.IsError())
        return status;
      if (!ParseFromValue(result.get(), &tmp_location)) {
        return Status(kUnknownError,
                      "failed to parse value of GET_LOCATION_IN_VIEW");
      }
      middle = tmp_location;
      middle.Offset(region.Width() / 2, region.Height() / 2);
      Timeout response_timeout(base::TimeDelta::FromSeconds(1));
      do {
        status =
         VerifyElementClickable(frame, web_view, clickable_element_id, middle);
        if (status.code() == kElementClickIntercepted)
          base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
        else
          break;
      } while (!response_timeout.IsExpired());
    }
    if (status.IsError())
      return status;
  }
  *location = tmp_location;
  return Status(kOk);
}

Status GetElementEffectiveStyle(
    const std::string& frame,
    WebView* web_view,
    const std::string& element_id,
    const std::string& property,
    std::string* value) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.AppendString(property);
  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      frame, webdriver::atoms::asString(webdriver::atoms::GET_EFFECTIVE_STYLE),
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsString(value)) {
    return Status(kUnknownError,
                  "failed to parse value of GET_EFFECTIVE_STYLE");
  }
  return Status(kOk);
}

Status GetElementBorder(
    const std::string& frame,
    WebView* web_view,
    const std::string& element_id,
    int* border_left,
    int* border_top) {
  std::string border_left_str;
  Status status = GetElementEffectiveStyle(
      frame, web_view, element_id, "border-left-width", &border_left_str);
  if (status.IsError())
    return status;
  std::string border_top_str;
  status = GetElementEffectiveStyle(
      frame, web_view, element_id, "border-top-width", &border_top_str);
  if (status.IsError())
    return status;
  int border_left_tmp = -1;
  int border_top_tmp = -1;
  base::StringToInt(border_left_str, &border_left_tmp);
  base::StringToInt(border_top_str, &border_top_tmp);
  if (border_left_tmp == -1 || border_top_tmp == -1)
    return Status(kUnknownError, "failed to get border width of element");
  std::string padding_left_str;
  status = GetElementEffectiveStyle(frame, web_view, element_id, "padding-left",
                                    &padding_left_str);
  int padding_left = 0;
  if (status.IsOk())
    base::StringToInt(padding_left_str, &padding_left);
  std::string padding_top_str;
  status = GetElementEffectiveStyle(frame, web_view, element_id, "padding-top",
                                    &padding_top_str);
  int padding_top = 0;
  if (status.IsOk())
    base::StringToInt(padding_top_str, &padding_top);
  *border_left = border_left_tmp + padding_left;
  *border_top = border_top_tmp + padding_top;
  return Status(kOk);
}

Status GetElementLocationInViewCenterHelper(const std::string& frame,
                                            WebView* web_view,
                                            const std::string& element_id,
                                            bool center,
                                            WebPoint* location) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.AppendBoolean(center);
  std::unique_ptr<base::Value> result;
  status =
      web_view->CallFunction(frame, kGetElementLocationScript, args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), location)) {
    return Status(kUnknownError,
                  "failed to parse value of getElementLocationInViewCenter");
  }
  return Status(kOk);
}

}  // namespace

std::string GetElementKey() {
  Session* session = GetThreadLocalSession();
  if (session && session->w3c_compliant)
    return kElementKeyW3C;
  else
    return kElementKey;
}

// example of element_id - d9cf1666-0066-4c07-bb86-03edcbab6680
// should contain only 0-9 or a-f
// format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
Status CheckElement(const std::string& element_id) {
  Session* session = GetThreadLocalSession();
  if (session && session->w3c_compliant) {
    if (element_id.length()!=36)
      return Status(kNoSuchElement, "Element_id length is invalid");

    for (std::string::size_type i=0; i<element_id.length(); ++i) {
      if (i==8 || i == 13 || i == 18 || i == 23) {
        if (element_id[i] != '-')
          return Status(kNoSuchElement, "Element_id format is invalid");
      } else {
        if (!(element_id[i] >='0' && element_id[i] <= '9') &&
            !(element_id[i] >='a' && element_id[i] <= 'f'))
          return Status(kNoSuchElement,
                        "Element_id contains invalid letter on position: " +
                         std::to_string(i) );
      }
    }
  }
  return Status(kOk);
}

std::unique_ptr<base::DictionaryValue> CreateElement(
    const std::string& element_id) {
  std::unique_ptr<base::DictionaryValue> element(new base::DictionaryValue());
  element->SetString(GetElementKey(), element_id);
  return element;
}

std::unique_ptr<base::DictionaryValue> CreateValueFrom(const WebPoint& point) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("x", point.x);
  dict->SetInteger("y", point.y);
  return dict;
}

Status FindElement(int interval_ms,
                   bool only_one,
                   const std::string* root_element_id,
                   Session* session,
                   WebView* web_view,
                   const base::DictionaryValue& params,
                   std::unique_ptr<base::Value>* value) {
  std::string strategy;
  if (!params.GetString("using", &strategy))
    return Status(kInvalidArgument, "'using' must be a string");
  if (session->w3c_compliant &&
      strategy != "css selector" &&
      strategy != "link text" &&
      strategy != "partial link text" &&
      strategy != "tag name" &&
      strategy != "xpath")
    return Status(kInvalidArgument, "invalid locator");

  std::string target;
  if (!params.GetString("value", &target))
    return Status(kInvalidArgument, "'value' must be a string");

  std::string script;
  if (only_one)
    script = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENT);
  else
    script = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENTS);
  std::unique_ptr<base::DictionaryValue> locator(new base::DictionaryValue());
  locator->SetString(strategy, target);
  base::ListValue arguments;
  arguments.Append(std::move(locator));
  if (root_element_id)
    arguments.Append(CreateElement(*root_element_id));

  base::TimeTicks start_time = base::TimeTicks::Now();
  int context_retry = 0;
  while (true) {
    std::unique_ptr<base::Value> temp;
    Status status = web_view->CallFunction(
        session->GetCurrentFrameId(), script, arguments, &temp);
    // A "Cannot find context" error can occur due to transition from in-process
    // iFrame to OOPIF. Retry a couple of times.
    if (status.IsError() &&
        (status.message().find("Cannot find context") == std::string::npos ||
         ++context_retry > 2)) {
      return status;
    }

    if (temp && !temp->is_none()) {
      if (only_one) {
        *value = std::move(temp);
        return Status(kOk);
      } else {
        base::ListValue* result;
        if (!temp->GetAsList(&result))
          return Status(kUnknownError, "script returns unexpected result");
        if (result->GetSize() > 0U) {
          *value = std::move(temp);
          return Status(kOk);
        }
      }
    }

    if (base::TimeTicks::Now() - start_time >= session->implicit_wait) {
      if (only_one) {
        return Status(kNoSuchElement, "Unable to locate element: {\"method\":\""
         + strategy + "\",\"selector\":\"" + target + "\"}");
      } else {
        value->reset(new base::ListValue());
        return Status(kOk);
      }
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(interval_ms));
  }

  return Status(kUnknownError);
}

Status GetActiveElement(Session* session,
                        WebView* web_view,
                        std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  Status status = web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function() { return document.activeElement || document.body }", args,
      value);
  if (status.IsError()) {
    return status;
  }
  if (value->get()->is_none()) {
    return Status(kNoSuchElement);
  }
  return status;
}

Status IsElementFocused(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_focused) {
  std::unique_ptr<base::Value> result;
  Status status = GetActiveElement(session, web_view, &result);
  if (status.IsError())
    return status;
  std::unique_ptr<base::Value> element_dict(CreateElement(element_id));
  *is_focused = result->Equals(element_dict.get());
  return Status(kOk);
}

Status IsDocumentTypeXml(
    Session* session,
    WebView* web_view,
    bool* is_xml_document) {

  std::unique_ptr<base::Value> contentType;
  Status status = web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      "document.contentType", &contentType);
  if (status.IsError())
          return status;
  if (base::LowerCaseEqualsASCII(contentType->GetString(),
                                 "text/xml"))
    *is_xml_document = true;
  else
    *is_xml_document = false;
  return Status(kOk);
}

Status GetElementAttribute(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const std::string& attribute_name,
                           std::unique_ptr<base::Value>* value) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.AppendString(attribute_name);
  return CallAtomsJs(
      session->GetCurrentFrameId(), web_view, webdriver::atoms::GET_ATTRIBUTE,
      args, value);
}

Status IsElementAttributeEqualToIgnoreCase(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const std::string& attribute_name,
    const std::string& attribute_value,
    bool* is_equal) {
  std::unique_ptr<base::Value> result;
  Status status = GetElementAttribute(
      session, web_view, element_id, attribute_name, &result);
  if (status.IsError())
    return status;
  std::string actual_value;
  if (result->GetAsString(&actual_value)) {
    *is_equal = base::LowerCaseEqualsASCII(actual_value, attribute_value);
  } else {
    *is_equal = false;
  }
  return status;
}

Status GetElementClickableLocation(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    WebPoint* location) {
  std::string tag_name;
  Status status = GetElementTagName(session, web_view, element_id, &tag_name);
  if (status.IsError())
    return status;
  std::string target_element_id = element_id;
  if (tag_name == "area") {
    // Scroll the image into view instead of the area.
    const char kGetImageElementForArea[] =
        "function (element) {"
        "  var map = element.parentElement;"
        "  if (map.tagName.toLowerCase() != 'map')"
        "    throw new Error('the area is not within a map');"
        "  var mapName = map.getAttribute('name');"
        "  if (mapName == null)"
        "    throw new Error ('area\\'s parent map must have a name');"
        "  mapName = '#' + mapName.toLowerCase();"
        "  var images = document.getElementsByTagName('img');"
        "  for (var i = 0; i < images.length; i++) {"
        "    if (images[i].useMap.toLowerCase() == mapName)"
        "      return images[i];"
        "  }"
        "  throw new Error('no img is found for the area');"
        "}";
    status = CheckElement(element_id);
    if (status.IsError())
      return status;
    base::ListValue args;
    args.Append(CreateElement(element_id));
    std::unique_ptr<base::Value> result;
    status = web_view->CallFunction(
        session->GetCurrentFrameId(), kGetImageElementForArea, args, &result);
    if (status.IsError())
      return status;
    const base::DictionaryValue* element_dict;
    if (!result->GetAsDictionary(&element_dict) ||
        !element_dict->GetString(GetElementKey(), &target_element_id))
      return Status(kUnknownError, "no element reference returned by script");
  }
  bool is_displayed = false;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (true) {
    Status status = IsElementDisplayed(
      session, web_view, target_element_id, true, &is_displayed);
    if (status.IsError())
      return status;
    if (is_displayed)
      break;
    if (base::TimeTicks::Now() - start_time >= session->implicit_wait) {
      return Status(kElementNotVisible);
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }

  WebRect rect;
  status = GetElementRegion(session, web_view, element_id, &rect);
  if (status.IsError())
    return status;

  status = ScrollElementRegionIntoView(
      session, web_view, target_element_id, rect,
      true /* center */, element_id, location);
  if (status.IsError())
    return status;
  location->Offset(rect.Width() / 2, rect.Height() / 2);
  return Status(kOk);
}

Status GetElementEffectiveStyle(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const std::string& property_name,
    std::string* property_value) {
  return GetElementEffectiveStyle(session->GetCurrentFrameId(), web_view,
                                  element_id, property_name, property_value);
}

Status GetElementRegion(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    WebRect* rect) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), kGetElementRegionScript, args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), rect)) {
    return Status(kUnknownError,
                  "failed to parse value of getElementRegion");
  }
  return Status(kOk);
}

Status GetElementTagName(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    std::string* name) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem) { return elem.tagName.toLowerCase(); }",
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsString(name))
    return Status(kUnknownError, "failed to get element tag name");
  return Status(kOk);
}

Status GetElementSize(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    WebSize* size) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  status = CallAtomsJs(
      session->GetCurrentFrameId(), web_view, webdriver::atoms::GET_SIZE,
      args, &result);
  if (status.IsError())
    return status;
  if (!ParseFromValue(result.get(), size))
    return Status(kUnknownError, "failed to parse value of GET_SIZE");
  return Status(kOk);
}

Status IsElementDisplayed(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool ignore_opacity,
    bool* is_displayed) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.AppendBoolean(ignore_opacity);
  std::unique_ptr<base::Value> result;
  status = CallAtomsJs(
      session->GetCurrentFrameId(), web_view, webdriver::atoms::IS_DISPLAYED,
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_displayed))
    return Status(kUnknownError, "IS_DISPLAYED should return a boolean value");
  return Status(kOk);
}

Status IsElementEnabled(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_enabled) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  status = CallAtomsJs(
      session->GetCurrentFrameId(), web_view, webdriver::atoms::IS_ENABLED,
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_enabled))
    return Status(kUnknownError, "IS_ENABLED should return a boolean value");
  return Status(kOk);
}

Status IsOptionElementSelected(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_selected) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  status = CallAtomsJs(
      session->GetCurrentFrameId(), web_view, webdriver::atoms::IS_SELECTED,
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_selected))
    return Status(kUnknownError, "IS_SELECTED should return a boolean value");
  return Status(kOk);
}

Status IsOptionElementTogglable(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool* is_togglable) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), kIsOptionElementToggleableScript,
      args, &result);
  if (status.IsError())
    return status;
  if (!result->GetAsBoolean(is_togglable))
    return Status(kUnknownError, "failed check if option togglable or not");
  return Status(kOk);
}

Status SetOptionElementSelected(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    bool selected) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  // TODO(171034): need to fix throwing error if an alert is triggered.
  base::ListValue args;
  args.Append(CreateElement(element_id));
  args.AppendBoolean(selected);
  std::unique_ptr<base::Value> result;
  return CallAtomsJs(
      session->GetCurrentFrameId(), web_view, webdriver::atoms::CLICK,
      args, &result);
}

Status ToggleOptionElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id) {
  bool is_selected;
  Status status = IsOptionElementSelected(
      session, web_view, element_id, &is_selected);
  if (status.IsError())
    return status;
  return SetOptionElementSelected(session, web_view, element_id, !is_selected);
}

Status ScrollElementIntoView(
    Session* session,
    WebView* web_view,
    const std::string& id,
    const WebPoint* offset,
    WebPoint* location) {
  WebRect region;
  Status status = GetElementRegion(session, web_view, id, &region);
  if (status.IsError())
    return status;
  status = ScrollElementRegionIntoView(session, web_view, id, region,
      false /* center */, std::string(), location);
  if (status.IsError())
    return status;
  if (offset)
    location->Offset(offset->x, offset->y);
  else
    location->Offset(region.size.width / 2, region.size.height / 2);
  return Status(kOk);
}

Status ScrollElementRegionIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const WebRect& region,
    bool center,
    const std::string& clickable_element_id,
    WebPoint* location) {
  WebPoint region_offset = region.origin;
  WebSize region_size = region.size;
  Status status = ScrollElementRegionIntoViewHelper(
      session->GetCurrentFrameId(), web_view, element_id, region,
      center, clickable_element_id, &region_offset);
  if (status.IsError())
    return status;
  const char kFindSubFrameScript[] =
      "function(xpath) {"
      "  return document.evaluate(xpath, document, null,"
      "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "}";
  for (auto rit = session->frames.rbegin(); rit != session->frames.rend();
       ++rit) {
    base::ListValue args;
    args.AppendString(
        base::StringPrintf("//*[@cd_frame_id_ = '%s']",
                           rit->chromedriver_frame_id.c_str()));
    std::unique_ptr<base::Value> result;
    status = web_view->CallFunction(
        rit->parent_frame_id, kFindSubFrameScript, args, &result);
    if (status.IsError())
      return status;
    const base::DictionaryValue* element_dict;
    if (!result->GetAsDictionary(&element_dict))
      return Status(kUnknownError, "no element reference returned by script");
    std::string frame_element_id;
    if (!element_dict->GetString(GetElementKey(), &frame_element_id))
      return Status(kUnknownError, "failed to locate a sub frame");

    // Modify |region_offset| by the frame's border.
    int border_left = -1;
    int border_top = -1;
    status = GetElementBorder(
        rit->parent_frame_id, web_view, frame_element_id,
        &border_left, &border_top);
    if (status.IsError())
      return status;
    region_offset.Offset(border_left, border_top);

    status = ScrollElementRegionIntoViewHelper(
        rit->parent_frame_id, web_view, frame_element_id,
        WebRect(region_offset, region_size),
        center, frame_element_id, &region_offset);
    if (status.IsError())
      return status;
  }
  *location = region_offset;
  return Status(kOk);
}

Status GetElementLocationInViewCenter(Session* session,
                                      WebView* web_view,
                                      const std::string& element_id,
                                      WebPoint* location) {
  WebPoint center_location;
  Status status = GetElementLocationInViewCenterHelper(
      session->GetCurrentFrameId(), web_view, element_id, true,
      &center_location);
  if (status.IsError())
    return status;
  const char kFindSubFrameScript[] =
      "function(xpath) {"
      "  return document.evaluate(xpath, document, null,"
      "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
      "}";
  for (auto rit = session->frames.rbegin(); rit != session->frames.rend();
       ++rit) {
    base::ListValue args;
    args.AppendString(base::StringPrintf("//*[@cd_frame_id_ = '%s']",
                                         rit->chromedriver_frame_id.c_str()));
    std::unique_ptr<base::Value> result;
    status = web_view->CallFunction(rit->parent_frame_id, kFindSubFrameScript,
                                    args, &result);
    if (status.IsError())
      return status;
    const base::DictionaryValue* element_dict;
    if (!result->GetAsDictionary(&element_dict))
      return Status(kUnknownError, "no element reference returned by script");
    std::string frame_element_id;
    if (!element_dict->GetString(GetElementKey(), &frame_element_id))
      return Status(kUnknownError, "failed to locate a sub frame");

    // Modify |center_location| by the frame's border.
    int border_left = -1;
    int border_top = -1;
    status = GetElementBorder(rit->parent_frame_id, web_view, frame_element_id,
                              &border_left, &border_top);
    if (status.IsError())
      return status;
    center_location.Offset(border_left, border_top);

    WebPoint frame_offset;
    status = GetElementLocationInViewCenterHelper(
        rit->parent_frame_id, web_view, frame_element_id, false, &frame_offset);
    if (status.IsError())
      return status;
    center_location.Offset(frame_offset.x, frame_offset.y);
  }
  *location = center_location;
  return Status(kOk);
}
