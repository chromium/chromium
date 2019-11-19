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
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "third_party/webdriver/atoms.h"

const int kFlickTouchEventsPerSecond = 30;
const std::set<std::string> textControlTypes = {"text", "search", "tel", "url",
                                                "password"};
const std::set<std::string> inputControlTypes = {
    "text",           "search", "url",   "tel",   "email",
    "password",       "date",   "month", "week",  "time",
    "datetime-local", "number", "range", "color", "file"};

namespace {

Status FocusToElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  bool is_displayed = false;
  bool is_focused = false;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (true) {
    status = IsElementDisplayed(
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
  status = IsElementEnabled(session, web_view, element_id, &is_enabled);
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
  return Status(kOk);
}

Status SendKeysToElement(Session* session,
                         WebView* web_view,
                         const std::string& element_id,
                         const bool is_text,
                         const base::ListValue* key_list) {
  // If we were previously focused, we don't need to focus again.
  // But also, later we don't move the carat if we were already in focus.
  // However, non-text elements such as contenteditable elements needs to be
  // focused to ensure the keys will end up being sent to the correct place.
  // So in the case of non-text elements, we still focusToElement.
  bool wasPreviouslyFocused = false;
  IsElementFocused(session, web_view, element_id, &wasPreviouslyFocused);
  if (!wasPreviouslyFocused || !is_text) {
    Status status = FocusToElement(session, web_view, element_id);
    if (status.IsError())
      return Status(kElementNotInteractable);
  }

  // Move cursor/caret to append the input if we only just focused this
  // element. keys if element's type is text-related
  if (is_text && !wasPreviouslyFocused) {
    base::ListValue args;
    args.Append(CreateElement(element_id));
    std::unique_ptr<base::Value> result;
    Status status = web_view->CallFunction(
        session->GetCurrentFrameId(),
        "elem => elem.setSelectionRange(elem.value.length, elem.value.length)",
        args, &result);
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
  return Status(kInvalidArgument, "element identifier must be a string");
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
    if (tag_name == "input") {
      std::unique_ptr<base::Value> get_element_type;
      status = GetElementAttribute(session, web_view, element_id, "type",
                                   &get_element_type);
      if (status.IsError())
        return status;
      std::string element_type;
      if (get_element_type->GetAsString(&element_type))
        element_type = base::ToLowerASCII(element_type);
      if (element_type == "file")
        return Status(kInvalidArgument);
    }
    WebPoint location;
    status = GetElementClickableLocation(
        session, web_view, element_id, &location);
    if (status.IsError())
      return status;

    std::list<MouseEvent> events;
    events.push_back(MouseEvent(kMovedMouseEventType, kNoneMouseButton,
                                location.x, location.y,
                                session->sticky_modifiers, 0, 0));
    events.push_back(MouseEvent(kPressedMouseEventType, kLeftMouseButton,
                                location.x, location.y,
                                session->sticky_modifiers, 0, 1));
    events.push_back(MouseEvent(kReleasedMouseEventType, kLeftMouseButton,
                                location.x, location.y,
                                session->sticky_modifiers, 1, 1));
    status = web_view->DispatchMouseEvents(events, session->GetCurrentFrameId(),
                                           false);
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
    return web_view->DispatchTouchEvents(events, false);
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
    return Status(kInvalidArgument, "'xoffset' must be an integer");
  if (!params.GetInteger("yoffset", &yoffset))
    return Status(kInvalidArgument, "'yoffset' must be an integer");
  if (!params.GetInteger("speed", &speed))
    return Status(kInvalidArgument, "'speed' must be an integer");
  if (speed < 1)
    return Status(kInvalidArgument, "'speed' must be a positive integer");

  status = web_view->DispatchTouchEvent(
      TouchEvent(kTouchStart, location.x, location.y), false);
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
        TouchEvent(kTouchMove, location.x + xoffset_per_event * i,
                   location.y + yoffset_per_event * i),
        false);
    if (status.IsError())
      return status;
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(1000 / kFlickTouchEventsPerSecond));
  }
  return web_view->DispatchTouchEvent(
      TouchEvent(kTouchEnd, location.x + xoffset, location.y + yoffset), false);
}

Status ExecuteClearElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;

  std::string tag_name;
  status = GetElementTagName(session, web_view, element_id, &tag_name);
  if (status.IsError())
    return status;
  std::string element_type;
  bool is_input_control = false;

  if (tag_name == "input") {
    std::unique_ptr<base::Value> get_element_type;
    status = GetElementAttribute(session, web_view, element_id, "type",
                                 &get_element_type);
    if (status.IsError())
      return status;
    if (get_element_type->GetAsString(&element_type))
      element_type = base::ToLowerASCII(element_type);

    is_input_control =
        inputControlTypes.find(element_type) != inputControlTypes.end();
  }

  bool is_text = tag_name == "textarea";
  bool is_content_editable = false;
  if (!is_text && !is_input_control) {
    std::unique_ptr<base::Value> get_content_editable;
    base::ListValue args;
    args.Append(CreateElement(element_id));
    status = web_view->CallFunction(session->GetCurrentFrameId(),
                                    "element => element.isContentEditable",
                                    args, &get_content_editable);
    if (status.IsError())
      return status;
    get_content_editable->GetAsBoolean(&is_content_editable);
  }

  std::unique_ptr<base::Value> get_readonly;
  bool is_readonly = false;
  base::DictionaryValue params_readOnly;
  if (!is_content_editable) {
    params_readOnly.SetString("name", "readOnly");
    status = ExecuteGetElementProperty(session, web_view, element_id,
                                       params_readOnly, &get_readonly);
    get_readonly->GetAsBoolean(&is_readonly);
    if (status.IsError())
      return status;
  }
  bool is_editable =
      (is_input_control || is_text || is_content_editable) && !is_readonly;
  if (!is_editable)
    return Status(kInvalidElementState);
  // Scrolling to element is done by webdriver::atoms::CLEAR
  bool is_displayed = false;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (true) {
    status = IsElementDisplayed(
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
  static bool isClearWarningNotified = false;
  if (!isClearWarningNotified) {
    VLOG(0) << "\n\t=== NOTE: ===\n"
            << "\tThe Clear command in " << kChromeDriverProductShortName
            << " 2.43 and above\n"
            << "\thas been updated to conform to the current standard,\n"
            << "\tincluding raising blur event after clearing.\n";
    isClearWarningNotified = true;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  const base::ListValue* key_list;
  base::ListValue key_list_local;
  if (session->w3c_compliant) {
    const base::Value* text;
    if (!params.Get("text", &text) || !text->is_string())
      return Status(kInvalidArgument, "'text' must be a string");
    key_list_local.Set(0, std::make_unique<base::Value>(text->Clone()));
    key_list = &key_list_local;
  } else {
    if (!params.GetList("value", &key_list))
      return Status(kInvalidArgument, "'value' must be a list");
  }

  bool is_input = false;
  status = IsElementAttributeEqualToIgnoreCase(session, web_view, element_id,
                                               "tagName", "input", &is_input);
  if (status.IsError())
    return status;
  std::unique_ptr<base::Value> get_element_type;
  status = GetElementAttribute(session, web_view, element_id, "type",
                               &get_element_type);
  if (status.IsError())
    return status;
  std::string element_type;
  if (get_element_type->GetAsString(&element_type))
    element_type = base::ToLowerASCII(element_type);
  bool is_file = element_type == "file";

  if (is_input && is_file) {
    if (session->strict_file_interactability) {
      status = FocusToElement(session, web_view,element_id);
      if (status.IsError())
        return status;
    }
    // Compress array into a single string.
    base::FilePath::StringType paths_string;
    for (size_t i = 0; i < key_list->GetSize(); ++i) {
      base::FilePath::StringType path_part;
      if (!key_list->GetString(i, &path_part))
        return Status(kInvalidArgument, "'value' is invalid");
      paths_string.append(path_part);
    }

    // w3c spec specifies empty path_part should throw invalidArgument error
    if (paths_string.empty())
      return Status(kInvalidArgument, "'text' is empty");

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
      return Status(kInvalidArgument,
                    "the element can not hold multiple files");

    std::unique_ptr<base::DictionaryValue> element(CreateElement(element_id));
    return web_view->SetFileInputFiles(session->GetCurrentFrameId(), *element,
                                       paths, multiple);
  } else {
    std::unique_ptr<base::Value> get_content_editable;
    base::ListValue args;
    args.Append(CreateElement(element_id));
    status = web_view->CallFunction(session->GetCurrentFrameId(),
                                    "element => element.isContentEditable",
                                    args, &get_content_editable);
    if (status.IsError())
      return status;
    bool is_content_editable;
    if (get_content_editable->GetAsBoolean(&is_content_editable) &&
        is_content_editable) {
      // If element is contentEditable, will move caret
      // at end of element text. W3C mandates that the
      // caret be moved "after any child content"
      std::unique_ptr<base::Value> result;
      status = web_view->CallFunction(
          session->GetCurrentFrameId(),
          "function(element) {"
          "var range = document.createRange();"
          "range.selectNodeContents(element);"
          "range.collapse();"
          "var sel = window.getSelection();"
          "sel.removeAllRanges();"
          "sel.addRange(range);"
          "while (element.parentElement && "
          "element.parentElement.isContentEditable) {"
          "    element = element.parentElement;"
          "  }"
          "return element;"
          "}",
          args, &result);
      if (status.IsError())
        return status;
      const base::DictionaryValue* element_dict;
      std::string target_element_id;
      if (!result->GetAsDictionary(&element_dict) ||
          !element_dict->GetString(GetElementKey(), &target_element_id))
        return Status(kUnknownError, "no element reference returned by script");
      return SendKeysToElement(session, web_view, target_element_id, false,
                               key_list);
    }
    // If element_type is in textControlTypes, sendKeys should append
    bool is_textControlType = is_input && textControlTypes.find(element_type) !=
                                              textControlTypes.end();
    // If the element is a textarea, sendKeys should also append
    bool is_textarea = false;
    status = IsElementAttributeEqualToIgnoreCase(
        session, web_view, element_id, "tagName", "textarea", &is_textarea);
    if (status.IsError())
      return status;
    bool is_text = is_textControlType || is_textarea;

    return SendKeysToElement(session, web_view, element_id, is_text, key_list);
  }
}

Status ExecuteSubmitElement(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));

  std::string name;
  if (!params.GetString("name", &name))
    return Status(kInvalidArgument, "missing 'name'");
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));

  bool is_xml = false;
  status = IsDocumentTypeXml(session, web_view, &is_xml);
  if (status.IsError())
    return status;

  if (is_xml) {
      value->reset(new base::Value(false));
      return Status(kOk);
  } else {
    return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_ENABLED),
      args,
      value);
  }
}

Status ExecuteIsElementDisplayed(Session* session,
                                 WebView* web_view,
                                 const std::string& element_id,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) {
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
  base::ListValue args;
  args.Append(CreateElement(element_id));

  std::unique_ptr<base::Value> location;
  status = web_view->CallFunction(
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
  Status status = CheckElement(element_id);
  if (status.IsError())
    return status;
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
    return Status(kInvalidArgument, "missing 'name'");
  return GetElementAttribute(session, web_view, element_id, name, value);
}

Status ExecuteGetElementValueOfCSSProperty(
                                      Session* session,
                                      WebView* web_view,
                                      const std::string& element_id,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value) {
  bool is_xml = false;
  Status status = IsDocumentTypeXml(session, web_view, &is_xml);
  if (status.IsError())
    return status;

  if (is_xml) {
      value->reset(new base::Value(""));
  } else {
    std::string property_name;
    if (!params.GetString("propertyName", &property_name))
      return Status(kInvalidArgument, "missing 'propertyName'");
    std::string property_value;
    status = GetElementEffectiveStyle(
        session, web_view, element_id, property_name, &property_value);
    if (status.IsError())
      return status;
    value->reset(new base::Value(property_value));
  }
  return Status(kOk);
}

Status ExecuteElementEquals(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  std::string other_element_id;
  if (!params.GetString("other", &other_element_id))
    return Status(kInvalidArgument, "'other' must be a string");
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
  // document.documentElement.clientHeight and Width provide viewport height
  // and width to crop screenshot if necessary.
  std::unique_ptr<base::Value> browser_info;
  status = web_view->EvaluateScript(
      std::string(),
      "({x: document.documentElement.scrollLeft || document.body.scrollLeft,"
      "  y: document.documentElement.scrollTop || document.body.scrollTop,"
      "  height: document.documentElement.clientHeight,"
      "  width: document.documentElement.clientWidth,"
      "  device_pixel_ratio: window.devicePixelRatio})",
      &browser_info);
  if (status.IsError())
    return status;

  double scroll_left = browser_info->FindKey("x")->GetDouble();
  double scroll_top = browser_info->FindKey("y")->GetDouble();
  double viewport_height = browser_info->FindKey("height")->GetDouble();
  double viewport_width = browser_info->FindKey("width")->GetDouble();
  double device_pixel_ratio =
         browser_info->FindKey("device_pixel_ratio")->GetDouble();

  std::unique_ptr<base::DictionaryValue> clip_dict =
      base::DictionaryValue::From(std::move(clip));
  if (!clip_dict)
    return Status(kUnknownError, "Element Rect is not a dictionary");
  // |clip_dict| already contains the right width and height of the target
  // element, but its x and y are relative to containing frame. We replace them
  // with the x and y relative to top-level document origin, as expected by
  // CaptureScreenshot.
  clip_dict->SetDouble("x", location.x + scroll_left);
  clip_dict->SetDouble("y", location.y + scroll_top);
  clip_dict->SetDouble("scale", 1 / device_pixel_ratio);
  // Crop screenshot by viewport if element is larger than viewport
  clip_dict->SetDouble(
      "height",
      std::min(viewport_height, clip_dict->FindKey("height")->GetDouble()));
  clip_dict->SetDouble(
      "width",
      std::min(viewport_width, clip_dict->FindKey("width")->GetDouble()));
  base::DictionaryValue screenshot_params;
  screenshot_params.SetDictionary("clip", std::move(clip_dict));

  std::string screenshot;
  status = web_view->CaptureScreenshot(&screenshot, screenshot_params);
  if (status.IsError())
    return status;

  value->reset(new base::Value(screenshot));
  return Status(kOk);
}
