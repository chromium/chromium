// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_commands.h"

#include <stddef.h>

#include <cmath>
#include <list>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "third_party/webdriver/atoms.h"

const int kFlickTouchEventsPerSecond = 30;

namespace {

Status SendKeysToElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::ListValue* key_list) {
  bool is_displayed = false;
  bool is_focused = false;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (true) {
    Status status = IsElementDisplayed(
        session, web_view, element_id, true, &is_displayed);
    if (status.IsError())
      return status;
    if (is_displayed)
      break;
    status = IsElementFocused(session, web_view, element_id, &is_focused);
    if (status.IsError())
      return status;
    if (is_focused)
      break;
    if (base::TimeTicks::Now() - start_time >= session->implicit_wait) {
      return Status(kElementNotVisible);
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }

  bool is_enabled = false;
  Status status = IsElementEnabled(session, web_view, element_id, &is_enabled);
  if (status.IsError())
    return status;
  if (!is_enabled)
    return Status(kInvalidElementState);

  if (!is_focused) {
    base::ListValue args;
    args.Append(CreateElement(element_id));
    std::unique_ptr<base::Value> result;
    status = web_view->CallFunction(
        session->GetCurrentFrameId(), kFocusScript, args, &result);
    if (status.IsError())
      return status;
  }

  return SendKeysOnWindow(web_view, key_list, true, &session->sticky_modifiers);
}

}  // namespace

Status ExecuteElementCommand(
    const ElementCommand& command,
    Session* session,
    WebView* web_view,
    const base::DictionaryValue& params,
    std::unique_ptr<base::Value>* value,
    Timeout* timeout) {
  std::string id;
  if (params.GetString("id", &id) || params.GetString("element", &id))
    return command.Run(session, web_view, id, params, value);
  return Status(kUnknownError, "element identifier must be a string");
}

Status ExecuteFindChildElement(int interval_ms,
                               Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, true, &element_id, session, web_view, params, value);
}

Status ExecuteFindChildElements(int interval_ms,
                                Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, false, &element_id, session, web_view, params, value);
}

Status ExecuteHoverOverElement(Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;

  MouseEvent move_event(
      kMovedMouseEventType, kNoneMouseButton, location.x, location.y,
      session->sticky_modifiers, 0);
  std::list<MouseEvent> events;
  events.push_back(move_event);
  status = web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
  if (status.IsOk())
    session->mouse_position = location;
  return status;
}

Status ExecuteClickElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  std::string tag_name;
  Status status = GetElementTagName(session, web_view, element_id, &tag_name);
  if (status.IsError())
    return status;
  if (tag_name == "option") {
    bool is_toggleable;
    status = IsOptionElementTogglable(
        session, web_view, element_id, &is_toggleable);
    if (status.IsError())
      return status;
    if (is_toggleable)
      return ToggleOptionElement(session, web_view, element_id);
    else
      return SetOptionElementSelected(session, web_view, element_id, true);
  } else {
    WebPoint location;
    status = GetElementClickableLocation(
        session, web_view, element_id, &location);
    if (status.IsError())
      return status;

    std::list<MouseEvent> events;
    events.push_back(
        MouseEvent(kMovedMouseEventType, kNoneMouseButton,
                   location.x, location.y, session->sticky_modifiers, 0));
    events.push_back(
        MouseEvent(kPressedMouseEventType, kLeftMouseButton,
                   location.x, location.y, session->sticky_modifiers, 1));
    events.push_back(
        MouseEvent(kReleasedMouseEventType, kLeftMouseButton,
                   location.x, location.y, session->sticky_modifiers, 1));
    status =
        web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
    if (status.IsOk())
      session->mouse_position = location;
    return status;
  }
}

Status ExecuteTouchSingleTap(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;
  if (!session->chrome->HasTouchScreen()) {
    // TODO(samuong): remove this once we stop supporting M44.
    std::list<TouchEvent> events;
    events.push_back(
        TouchEvent(kTouchStart, location.x, location.y));
    events.push_back(
        TouchEvent(kTouchEnd, location.x, location.y));
    return web_view->DispatchTouchEvents(events);
  }
  return web_view->SynthesizeTapGesture(location.x, location.y, 1, false);
}

Status ExecuteTouchDoubleTap(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  if (!session->chrome->HasTouchScreen()) {
    // TODO(samuong): remove this once we stop supporting M44.
    return Status(kUnknownCommand, "Double tap command requires Chrome 44+");
  }
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;
  return web_view->SynthesizeTapGesture(location.x, location.y, 2, false);
}

Status ExecuteTouchLongPress(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  if (!session->chrome->HasTouchScreen()) {
    // TODO(samuong): remove this once we stop supporting M44.
    return Status(kUnknownCommand, "Long press command requires Chrome 44+");
  }
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;
  return web_view->SynthesizeTapGesture(location.x, location.y, 1, true);
}

Status ExecuteFlick(Session* session,
                    WebView* web_view,
                    const std::string& element_id,
                    const base::DictionaryValue& params,
                    std::unique_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;

  int xoffset, yoffset, speed;
  if (!params.GetInteger("xoffset", &xoffset))
    return Status(kUnknownError, "'xoffset' must be an integer");
  if (!params.GetInteger("yoffset", &yoffset))
    return Status(kUnknownError, "'yoffset' must be an integer");
  if (!params.GetInteger("speed", &speed))
    return Status(kUnknownError, "'speed' must be an integer");
  if (speed < 1)
    return Status(kUnknownError, "'speed' must be a positive integer");

  status = web_view->DispatchTouchEvent(
      TouchEvent(kTouchStart, location.x, location.y));
  if (status.IsError())
    return status;

  const double offset =
      std::sqrt(static_cast<double>(xoffset * xoffset + yoffset * yoffset));
  const double xoffset_per_event =
      (speed * xoffset) / (kFlickTouchEventsPerSecond * offset);
  const double yoffset_per_event =
      (speed * yoffset) / (kFlickTouchEventsPerSecond * offset);
  const int total_events =
      (offset * kFlickTouchEventsPerSecond) / speed;
  for (int i = 0; i < total_events; i++) {
    status = web_view->DispatchTouchEvent(
        TouchEvent(kTouchMove,
                   location.x + xoffset_per_event * i,
                   location.y + yoffset_per_event * i));
    if (status.IsError())
      return status;
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(1000 / kFlickTouchEventsPerSecond));
  }
  return web_view->DispatchTouchEvent(
      TouchEvent(kTouchEnd, location.x + xoffset, location.y + yoffset));
}

Status ExecuteClearElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  // Scrolling to element is done by webdriver::atoms::CLEAR
  bool is_displayed = false;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (true) {
    Status status = IsElementDisplayed(
      session, web_view, element_id, true, &is_displayed);
    if (status.IsError())
      return status;
    if (is_displayed)
      break;
    if (base::TimeTicks::Now() - start_time >= session->implicit_wait) {
      return Status(kElementNotVisible);
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
  }
  base::ListValue args;
  args.Append(CreateElement(element_id));
  std::unique_ptr<base::Value> result;
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::CLEAR),
      args, &result);
}

Status ExecuteSendKeysToElement(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  const base::ListValue* key_list;
  if (!params.GetList("value", &key_list))
    return Status(kUnknownError, "'value' must be a list");

  bool is_input = false;
  Status status = IsElementAttributeEqualToIgnoreCase(
      session, web_view, element_id, "tagName", "input", &is_input);
  if (status.IsError())
    return status;
  bool is_file = false;
  status = IsElementAttributeEqualToIgnoreCase(
      session, web_view, element_id, "type", "file", &is_file);
  if (status.IsError())
    return status;
  if (is_input && is_file) {
    // Compress array into a single string.
    base::FilePath::StringType paths_string;
    for (size_t i = 0; i < key_list->GetSize(); ++i) {
      base::FilePath::StringType path_part;
      if (!key_list->GetString(i, &path_part))
        return Status(kUnknownError, "'value' is invalid");
      paths_string.append(path_part);
    }

    ChromeDesktopImpl* chrome_desktop = nullptr;
    bool is_desktop = session->chrome->GetAsDesktop(&chrome_desktop).IsOk();

    // Separate the string into separate paths, delimited by '\n'.
    std::vector<base::FilePath> paths;
    for (const auto& path_piece : base::SplitStringPiece(
             paths_string, base::FilePath::StringType(1, '\n'),
             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      // For local desktop browser, verify that the file exists.
      // No easy way to do that for remote or mobile browser.
      if (is_desktop && !base::PathExists(base::FilePath(path_piece))) {
        return Status(
            kInvalidArgument,
            base::StringPrintf("File not found : %" PRFilePath,
                               base::FilePath(path_piece).value().c_str()));
      }
      paths.push_back(base::FilePath(path_piece));
    }

    bool multiple = false;
    status = IsElementAttributeEqualToIgnoreCase(
        session, web_view, element_id, "multiple", "true", &multiple);
    if (status.IsError())
      return status;
    if (!multiple && paths.size() > 1)
      return Status(kUnknownError, "the element can not hold multiple files");

    std::unique_ptr<base::DictionaryValue> element(CreateElement(element_id));
    return web_view->SetFileInputFiles(
        session->GetCurrentFrameId(), *element, paths);
  } else {
    return SendKeysToElement(session, web_view, element_id, key_list);
  }
}

Status ExecuteSubmitElement(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::SUBMIT),
      args,
      value);
}

Status ExecuteGetElementText(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_TEXT),
      args,
      value);
}

Status ExecuteGetElementValue(Session* session,
                              WebView* web_view,
                              const std::string& element_id,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem) { return elem['value'] }",
      args,
      value);
}

Status ExecuteGetElementProperty(Session* session,
                              WebView* web_view,
                              const std::string& element_id,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));

  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "missing 'name'");
  args.AppendString(name);

  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem, name) { return elem[name] }",
      args,
      value);
}

Status ExecuteGetElementTagName(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem) { return elem.tagName.toLowerCase() }",
      args,
      value);
}

Status ExecuteIsElementSelected(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_SELECTED),
      args,
      value);
}

Status ExecuteIsElementEnabled(Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_ENABLED),
      args,
      value);
}

Status ExecuteIsElementDisplayed(Session* session,
                                 WebView* web_view,
                                 const std::string& element_id,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_DISPLAYED),
      args,
      value);
}

Status ExecuteGetElementLocation(Session* session,
                                 WebView* web_view,
                                 const std::string& element_id,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_LOCATION),
      args,
      value);
}

Status ExecuteGetElementRect(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));

  std::unique_ptr<base::Value> location;
  Status status = web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_LOCATION), args,
      &location);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> size;
  web_view->CallFunction(session->GetCurrentFrameId(),
                         webdriver::atoms::asString(webdriver::atoms::GET_SIZE),
                         args, &size);

  // do type conversions
  base::DictionaryValue* size_dict;
  if (!size->GetAsDictionary(&size_dict))
    return Status(kUnknownError, "could not convert to DictionaryValue");
  base::DictionaryValue* location_dict;
  if (!location->GetAsDictionary(&location_dict))
    return Status(kUnknownError, "could not convert to DictionaryValue");

  // grab values
  double x, y, width, height;
  if (!location_dict->GetDouble("x", &x))
    return Status(kUnknownError, "x coordinate is missing in element location");

  if (!location_dict->GetDouble("y", &y))
    return Status(kUnknownError, "y coordinate is missing in element location");

  if (!size_dict->GetDouble("height", &height))
    return Status(kUnknownError, "height is missing in element size");

  if (!size_dict->GetDouble("width", &width))
    return Status(kUnknownError, "width is missing in element size");

  base::DictionaryValue ret;
  ret.SetDouble("x", x);
  ret.SetDouble("y", y);
  ret.SetDouble("width", width);
  ret.SetDouble("height", height);
  value->reset(ret.DeepCopy());
  return Status(kOk);
}

Status ExecuteGetElementLocationOnceScrolledIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    std::unique_ptr<base::Value>* value) {
  WebPoint offset(0, 0);
  WebPoint location;
  Status status = ScrollElementIntoView(
      session, web_view, element_id, &offset, &location);
  if (status.IsError())
    return status;
  *value = CreateValueFrom(location);
  return Status(kOk);
}

Status ExecuteGetElementSize(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  base::ListValue args;
  args.Append(CreateElement(element_id));
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_SIZE),
      args,
      value);
}

Status ExecuteGetElementAttribute(Session* session,
                                  WebView* web_view,
                                  const std::string& element_id,
                                  const base::DictionaryValue& params,
                                  std::unique_ptr<base::Value>* value) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "missing 'name'");
  return GetElementAttribute(session, web_view, element_id, name, value);
}

Status ExecuteGetElementValueOfCSSProperty(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::DictionaryValue& params,
    std::unique_ptr<base::Value>* value) {
  std::string property_name;
  if (!params.GetString("propertyName", &property_name))
    return Status(kUnknownError, "missing 'propertyName'");
  std::string property_value;
  Status status = GetElementEffectiveStyle(
      session, web_view, element_id, property_name, &property_value);
  if (status.IsError())
    return status;
  value->reset(new base::Value(property_value));
  return Status(kOk);
}

Status ExecuteElementEquals(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  std::string other_element_id;
  if (!params.GetString("other", &other_element_id))
    return Status(kUnknownError, "'other' must be a string");
  value->reset(new base::Value(element_id == other_element_id));
  return Status(kOk);
}

Status ExecuteElementScreenshot(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  Status status = session->chrome->ActivateWebView(web_view->GetId());
  if (status.IsError())
    return status;

  WebPoint offset(0, 0);
  WebPoint location;
  status =
      ScrollElementIntoView(session, web_view, element_id, &offset, &location);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> clip;
  status = ExecuteGetElementRect(session, web_view, element_id, params, &clip);
  if (status.IsError())
    return status;

  // |location| returned by ScrollElementIntoView is relative to the current
  // view port. However, CaptureScreenshot expects a location relative to the
  // document origin. We make the adjustment using the scroll amount of the top
  // level window. Scrolling of frames has already been included in |location|.
  // Scroll information can be in either document.documentElement or
  // document.body, depending on document compatibility mode. The parentheses
  // around the JavaScript code below is needed because JavaScript syntax
  // doesn't allow a statement to start with an object literal.
  std::unique_ptr<base::Value> scroll;
  status = web_view->EvaluateScript(
      std::string(),
      "({x: document.documentElement.scrollLeft || document.body.scrollLeft,"
      "  y: document.documentElement.scrollTop || document.body.scrollTop})",
      &scroll);
  if (status.IsError())
    return status;
  int scroll_left = scroll->FindKey("x")->GetInt();
  int scroll_top = scroll->FindKey("y")->GetInt();

  std::unique_ptr<base::DictionaryValue> clip_dict =
      base::DictionaryValue::From(std::move(clip));
  if (!clip_dict)
    return Status(kUnknownError, "Element Rect is not a dictionary");
  // |clip_dict| already contains the right width and height of the target
  // element, but its x and y are relative to containing frame. We replace them
  // with the x and y relative to top-level document origin, as expected by
  // CaptureScreenshot.
  clip_dict->SetInteger("x", location.x + scroll_left);
  clip_dict->SetInteger("y", location.y + scroll_top);
  clip_dict->SetDouble("scale", 1.0);
  base::DictionaryValue screenshot_params;
  screenshot_params.SetDictionary("clip", std::move(clip_dict));

  std::string screenshot;
  status = web_view->CaptureScreenshot(&screenshot, screenshot_params);
  if (status.IsError())
    return status;

  value->reset(new base::Value(screenshot));
  return Status(kOk);
}
