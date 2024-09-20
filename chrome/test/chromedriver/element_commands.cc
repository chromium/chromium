// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/element_commands.h"

#include <stddef.h>

#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
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
#include "third_party/selenium-atoms/atoms.h"

const int kFlickTouchEventsPerSecond = 30;
const std::set<std::string> kTextControlTypes = {"text", "search", "tel", "url",
                                                 "password"};
const std::set<std::string> kInputControlTypes = {
    "text",           "search", "url",   "tel",   "email",
    "password",       "date",   "month", "week",  "time",
    "datetime-local", "number", "range", "color", "file"};

const std::set<std::string> kNontypeableControlTypes = {"color"};

const std::unordered_set<std::string> kBooleanAttributes = {
    "allowfullscreen",
    "allowpaymentrequest",
    "allowusermedia",
    "async",
    "autofocus",
    "autoplay",
    "checked",
    "compact",
    "complete",
    "controls",
    "declare",
    "default",
    "defaultchecked",
    "defaultselected",
    "defer",
    "disabled",
    "ended",
    "formnovalidate",
    "hidden",
    "indeterminate",
    "iscontenteditable",
    "ismap",
    "itemscope",
    "loop",
    "multiple",
    "muted",
    "nohref",
    "nomodule",
    "noresize",
    "noshade",
    "novalidate",
    "nowrap",
    "open",
    "paused",
    "playsinline",
    "pubdate",
    "readonly",
    "required",
    "reversed",
    "scoped",
    "seamless",
    "seeking",
    "selected",
    "truespeed",
    "typemustmatch",
    "willvalidate"};

namespace {

Status FocusToElement(
    Session* session,
    WebView* web_view,
    const std::string& element_id) {
  Status status{kOk};
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
    base::PlatformThread::Sleep(base::Milliseconds(100));
  }

  bool is_enabled = false;
  status = IsElementEnabled(session, web_view, element_id, &is_enabled);
  if (status.IsError())
    return status;
  if (!is_enabled)
    return Status(kInvalidElementState);

  if (!is_focused) {
    base::Value::List args;
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
                         const base::Value::List* key_list) {
  // If we were previously focused, we don't need to focus again.
  // But also, later we don't move the carat if we were already in focus.
  // However, non-text elements such as contenteditable elements needs to be
  // focused to ensure the keys will end up being sent to the correct place.
  // So in the case of non-text elements, we still focusToElement.
  bool was_previously_focused = false;
  IsElementFocused(session, web_view, element_id, &was_previously_focused);
  if (!was_previously_focused || !is_text) {
    Status status = FocusToElement(session, web_view, element_id);
    if (status.IsError())
      return Status(kElementNotInteractable);
  }

  // Move cursor/caret to append the input if we only just focused this
  // element. keys if element's type is text-related
  if (is_text && !was_previously_focused) {
    base::Value::List args;
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

Status ExecuteElementCommand(const ElementCommand& command,
                             Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  const std::string* id = params.FindString("id");
  if (!id)
    id = params.FindString("element");
  if (id) {
    return command.Run(session, web_view, *id, params, value);
  }
  return Status(kInvalidArgument, "element identifier must be a string");
}

Status ExecuteFindChildElement(int interval_ms,
                               Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, true, &element_id, session, web_view, params, value);
}

Status ExecuteFindChildElementFromShadowRoot(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const std::string& shadow_root_id,
    const base::Value::Dict& params,
    std::unique_ptr<base::Value>* value) {
  return FindShadowElement(interval_ms, true, &shadow_root_id, session,
                           web_view, params, value);
}

Status ExecuteFindChildElementsFromShadowRoot(
    int interval_ms,
    Session* session,
    WebView* web_view,
    const std::string& shadow_root_id,
    const base::Value::Dict& params,
    std::unique_ptr<base::Value>* value) {
  return FindShadowElement(interval_ms, false, &shadow_root_id, session,
                           web_view, params, value);
}

Status ExecuteGetElementShadowRoot(Session* session,
                                   WebView* web_view,
                                   const std::string& element_id,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value) {
  base::Value::List args;
  args.Append(CreateElement(element_id));

  Status status = web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem) { return elem.shadowRoot; }", args, value);

  if (status.IsError()) {
    if (status.message().find("no such shadow root") != std::string::npos) {
      return Status(kNoSuchShadowRoot);
    }

    return status;
  }

  if (value->get()->is_none()) {
    return Status(kNoSuchShadowRoot);
  }

  return status;
}

Status ExecuteFindChildElements(int interval_ms,
                                Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value) {
  return FindElement(
      interval_ms, false, &element_id, session, web_view, params, value);
}

Status ExecuteClickElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::Value::Dict& params,
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
    return SetOptionElementSelected(session, web_view, element_id, true);
  }

  if (tag_name == "input") {
    std::unique_ptr<base::Value> get_element_type;
    status = GetElementAttribute(session, web_view, element_id, "type",
                                 &get_element_type);
    if (status.IsError())
      return status;
    std::string element_type;
    if (get_element_type->is_string())
      element_type = base::ToLowerASCII(get_element_type->GetString());
    if (element_type == "file")
      return Status(kInvalidArgument);
  }
  WebPoint absolute_location;
  status = GetElementClickableLocation(session, web_view, element_id,
                                       &absolute_location);
  if (status.IsError())
    return status;

  WebView* containing_web_view =
      web_view->FindContainerForFrame(session->GetCurrentFrameId());

  WebPoint relative_location;
  status = GetElementClickableLocation(session, containing_web_view, element_id,
                                       &relative_location);
  if (status.IsError()) {
    return status;
  }

  std::vector<MouseEvent> events;
  events.emplace_back(kMovedMouseEventType, kNoneMouseButton,
                      relative_location.x, relative_location.y,
                      session->sticky_modifiers, 0, 0);
  events.emplace_back(kPressedMouseEventType, kLeftMouseButton,
                      relative_location.x, relative_location.y,
                      session->sticky_modifiers, 0, 1);
  events.emplace_back(kReleasedMouseEventType, kLeftMouseButton,
                      relative_location.x, relative_location.y,
                      session->sticky_modifiers, 1, 1);
  status = containing_web_view->DispatchMouseEvents(
      events, session->GetCurrentFrameId(), false);
  if (status.IsOk())
    session->mouse_position = absolute_location;
  return status;
}

Status ExecuteTouchSingleTap(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;
  if (!session->chrome->HasTouchScreen()) {
    // TODO(samuong): remove this once we stop supporting M44.
    std::vector<TouchEvent> events;
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
                             const base::Value::Dict& params,
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
                             const base::Value::Dict& params,
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
                    const base::Value::Dict& params,
                    std::unique_ptr<base::Value>* value) {
  WebPoint location;
  Status status = GetElementClickableLocation(
      session, web_view, element_id, &location);
  if (status.IsError())
    return status;

  int xoffset, yoffset, speed;

  std::optional<int> maybe_xoffset = params.FindInt("xoffset");
  if (!maybe_xoffset)
    return Status(kInvalidArgument, "'xoffset' must be an integer");
  xoffset = *maybe_xoffset;

  std::optional<int> maybe_yoffset = params.FindInt("yoffset");
  if (!maybe_yoffset)
    return Status(kInvalidArgument, "'yoffset' must be an integer");
  yoffset = *maybe_yoffset;

  speed = params.FindInt("speed").value_or(-1);
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
        base::Milliseconds(1000 / kFlickTouchEventsPerSecond));
  }
  return web_view->DispatchTouchEvent(
      TouchEvent(kTouchEnd, location.x + xoffset, location.y + yoffset), false);
}

Status ExecuteClearElement(Session* session,
                           WebView* web_view,
                           const std::string& element_id,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value) {
  std::string tag_name;
  Status status = GetElementTagName(session, web_view, element_id, &tag_name);
  if (status.IsError())
    return status;
  bool is_input_control = false;

  if (tag_name == "input") {
    std::unique_ptr<base::Value> get_element_type;
    status = GetElementAttribute(session, web_view, element_id, "type",
                                 &get_element_type);
    if (status.IsError())
      return status;

    std::string element_type;
    if (get_element_type->is_string())
      element_type = base::ToLowerASCII(get_element_type->GetString());

    is_input_control =
        kInputControlTypes.find(element_type) != kInputControlTypes.end();
  }

  bool is_text = tag_name == "textarea";
  bool is_content_editable = false;
  if (!is_text && !is_input_control) {
    std::unique_ptr<base::Value> get_content_editable;
    base::Value::List args;
    args.Append(CreateElement(element_id));
    status = web_view->CallFunction(session->GetCurrentFrameId(),
                                    "element => element.isContentEditable",
                                    args, &get_content_editable);
    if (status.IsError())
      return status;
    is_content_editable = get_content_editable->GetIfBool().value_or(false);
  }

  std::unique_ptr<base::Value> get_readonly;
  bool is_readonly = false;
  base::Value::Dict params_readOnly;
  if (!is_content_editable) {
    params_readOnly.Set("name", "readOnly");
    status = ExecuteGetElementProperty(session, web_view, element_id,
                                       params_readOnly, &get_readonly);
    if (status.IsError())
      return status;
    is_readonly = get_readonly->GetIfBool().value_or(false);
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
    base::PlatformThread::Sleep(base::Milliseconds(50));
  }
  static bool is_clear_warning_notified = false;
  if (!is_clear_warning_notified) {
    VLOG(0) << "\n\t=== NOTE: ===\n"
            << "\tThe Clear command in " << kChromeDriverProductShortName
            << " 2.43 and above\n"
            << "\thas been updated to conform to the current standard,\n"
            << "\tincluding raising blur event after clearing.\n";
    is_clear_warning_notified = true;
  }
  base::Value::List args;
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
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value) {
  const base::Value::List* key_list;
  base::Value::List key_list_local;
  const base::Value* text = nullptr;
  if (session->w3c_compliant) {
    text = params.Find("text");
    if (text == nullptr || !text->is_string())
      return Status(kInvalidArgument, "'text' must be a string");
    key_list_local.Append(text->Clone());
    key_list = &key_list_local;
  } else {
    key_list = params.FindList("value");
    if (key_list == nullptr) {
      return Status(kInvalidArgument, "'value' must be a list");
    }
  }

  bool is_input = false;
  Status status = IsElementAttributeEqualToIgnoreCase(
      session, web_view, element_id, "tagName", "input", &is_input);
  if (status.IsError())
    return status;
  std::unique_ptr<base::Value> get_element_type;
  status = GetElementAttribute(session, web_view, element_id, "type",
                               &get_element_type);
  if (status.IsError())
    return status;
  std::string element_type;
  if (get_element_type->is_string())
    element_type = base::ToLowerASCII(get_element_type->GetString());
  bool is_file = element_type == "file";
  bool is_nontypeable = kNontypeableControlTypes.find(element_type) !=
                        kNontypeableControlTypes.end();

  if (is_input && is_file) {
    if (session->strict_file_interactability) {
      status = FocusToElement(session, web_view,element_id);
      if (status.IsError())
        return status;
    }
    // Compress array into a single string.
    std::string paths_string;
    for (const base::Value& i : *key_list) {
      const std::string* path_part = i.GetIfString();
      if (!path_part)
        return Status(kInvalidArgument, "'value' is invalid");
      paths_string.append(*path_part);
    }

    // w3c spec specifies empty path_part should throw invalidArgument error
    if (paths_string.empty())
      return Status(kInvalidArgument, "'text' is empty");

    ChromeDesktopImpl* chrome_desktop = nullptr;
    bool is_desktop = session->chrome->GetAsDesktop(&chrome_desktop).IsOk();

    // Separate the string into separate paths, delimited by '\n'.
    std::vector<base::FilePath> paths;
    for (const auto& path_piece : base::SplitStringPiece(
             paths_string, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      // For local desktop browser, verify that the file exists.
      // No easy way to do that for remote or mobile browser.
      if (is_desktop &&
          !base::PathExists(base::FilePath::FromUTF8Unsafe(path_piece))) {
        return Status(
            kInvalidArgument,
            base::StringPrintf(
                "File not found : %" PRFilePath,
                base::FilePath::FromUTF8Unsafe(path_piece).value().c_str()));
      }
      paths.push_back(base::FilePath::FromUTF8Unsafe(path_piece));
    }

    bool multiple = false;
    status = IsElementAttributeEqualToIgnoreCase(
        session, web_view, element_id, "multiple", "true", &multiple);
    if (status.IsError())
      return status;
    if (!multiple && paths.size() > 1) {
      return Status(kInvalidArgument,
                    "the element can not hold multiple files");
    }

    base::Value element = CreateElement(element_id);
    return web_view->SetFileInputFiles(session->GetCurrentFrameId(), element,
                                       paths, multiple);
  }

  if (session->w3c_compliant && is_input && is_nontypeable) {
    // Special handling for non-typeable inputs is only included in W3C Spec
    // The Spec calls for returning element not interactable if the element
    // has no value property, but this is included for all input elements, so
    // no check is needed here.

    // text is set only when session.w3c_compliant, so confirm here
    DCHECK(text != nullptr);
    base::Value::List args;
    args.Append(CreateElement(element_id));
    args.Append(text->GetString());
    std::unique_ptr<base::Value> result;
    // Set value to text as given by user; if this does not match the defined
    // format for the input type, results are not defined
    return web_view->CallFunction(session->GetCurrentFrameId(),
                                  "(element, text) => element.value = text",
                                  args, &result);
  }

  std::unique_ptr<base::Value> get_content_editable;
  base::Value::List args;
  args.Append(CreateElement(element_id));
  status = web_view->CallFunction(session->GetCurrentFrameId(),
                                  "element => element.isContentEditable", args,
                                  &get_content_editable);
  if (status.IsError())
    return status;

  // If element_type is in kTextControlTypes, sendKeys should append
  bool is_text_control_type =
      is_input &&
      kTextControlTypes.find(element_type) != kTextControlTypes.end();
  // If the element is a textarea, sendKeys should also append
  bool is_textarea = false;
  status = IsElementAttributeEqualToIgnoreCase(
      session, web_view, element_id, "tagName", "textarea", &is_textarea);
  if (status.IsError())
    return status;
  bool is_text = is_text_control_type || is_textarea;

  if (get_content_editable->is_bool() && get_content_editable->GetBool()) {
    // If element is contentEditable
    // check if element is focused
    bool is_focused = false;
    status = IsElementFocused(session, web_view, element_id, &is_focused);
    if (status.IsError())
      return status;

    // Get top level contentEditable element
    std::unique_ptr<base::Value> result;
    status = web_view->CallFunction(session->GetCurrentFrameId(),
                                    "function(element) {"
                                    "while (element.parentElement && "
                                    "element.parentElement.isContentEditable) {"
                                    "    element = element.parentElement;"
                                    "  }"
                                    "return element;"
                                    "}",
                                    args, &result);
    if (status.IsError())
      return status;
    const base::Value::Dict* element_dict = result->GetIfDict();
    const std::string* top_element_id =
        element_dict ? element_dict->FindString(GetElementKey()) : nullptr;
    if (!top_element_id)
      return Status(kUnknownError, "no element reference returned by script");

    // check if top level contentEditable element is focused
    bool is_top_focused = false;
    status =
        IsElementFocused(session, web_view, *top_element_id, &is_top_focused);
    if (status.IsError())
      return status;
    // If is_text we want to send keys to the element
    // Otherwise, send keys to the top element
    if ((is_text && !is_focused) || (!is_text && !is_top_focused)) {
      // If element does not currentley have focus
      // will move caret
      // at end of element text. W3C mandates that the
      // caret be moved "after any child content"
      // Set selection using the element itself
      std::unique_ptr<base::Value> unused;
      status = web_view->CallFunction(session->GetCurrentFrameId(),
                                      "function(element) {"
                                      "var range = document.createRange();"
                                      "range.selectNodeContents(element);"
                                      "range.collapse();"
                                      "var sel = window.getSelection();"
                                      "sel.removeAllRanges();"
                                      "sel.addRange(range);"
                                      "}",
                                      args, &unused);
      if (status.IsError())
        return status;
    }
    // Use top level element id for the purpose of focusing
    if (!is_text) {
      return SendKeysToElement(session, web_view, *top_element_id, is_text,
                               key_list);
    }
  }
  return SendKeysToElement(session, web_view, element_id, is_text, key_list);
}

Status ExecuteSubmitElement(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) {
  base::Value::List args;
  args.Append(CreateElement(element_id));

  const std::string* name = params.FindString("name");
  if (!name)
    return Status(kInvalidArgument, "missing 'name'");
  args.Append(*name);

  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      "function(elem, name) { return elem[name] }",
      args,
      value);
}

Status ExecuteGetElementTagName(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value) {
  base::Value::List args;
  args.Append(CreateElement(element_id));

  bool is_xml = false;
  Status status = IsDocumentTypeXml(session, web_view, &is_xml);
  if (status.IsError())
    return status;

  if (is_xml) {
    *value = std::make_unique<base::Value>(false);
    return Status(kOk);
  }
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::IS_ENABLED), args, value);
}

Status ExecuteGetComputedLabel(Session* session,
                               WebView* web_view,
                               const std::string& element_id,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::Value> ax_node;
  Status status = GetAXNodeByElementId(session, web_view, element_id, &ax_node);
  if (status.IsError())
    return status;

  // Computed label stores as `name` in the AXTree.
  base::Value::Dict* name_node = ax_node->GetDict().FindDict("name");
  if (!name_node) {
    // No computed label found. Return empty string.
    *value = std::make_unique<base::Value>("");
    return Status(kOk);
  }

  std::optional<base::Value> name_val = name_node->Extract("value");
  if (!name_val)
    return Status(kUnknownError,
                  "No name value found in the node in CDP response");

  *value = std::make_unique<base::Value>(std::move(*name_val));

  return Status(kOk);
}

Status ExecuteGetComputedRole(Session* session,
                              WebView* web_view,
                              const std::string& element_id,
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::Value> ax_node;
  Status status = GetAXNodeByElementId(session, web_view, element_id, &ax_node);
  if (status.IsError())
    return status;

  base::Value::Dict* role_node = ax_node->GetDict().FindDict("role");
  if (!role_node) {
    // No computed role found. Return empty string.
    *value = std::make_unique<base::Value>("");
    return Status(kOk);
  }

  std::optional<base::Value> role_val = role_node->Extract("value");
  if (!role_val) {
    return Status(kUnknownError,
                  "No role value found in the node in CDP response");
  }

  *value = std::make_unique<base::Value>(std::move(*role_val));

  return Status(kOk);
}

Status ExecuteIsElementDisplayed(Session* session,
                                 WebView* web_view,
                                 const std::string& element_id,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  base::Value::List args;
  args.Append(CreateElement(element_id));

  std::unique_ptr<base::Value> location;
  Status status = web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_LOCATION), args,
      &location);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> size;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(),
      webdriver::atoms::asString(webdriver::atoms::GET_SIZE), args, &size);
  if (status.IsError())
    return status;

  // do type conversions
  base::Value::Dict* size_dict = size->GetIfDict();
  if (!size_dict)
    return Status(kUnknownError, "could not convert to Value::Dict");
  base::Value::Dict* location_dict = location->GetIfDict();
  if (!location_dict)
    return Status(kUnknownError, "could not convert to Value::Dict");

  // grab values
  std::optional<double> maybe_x = location_dict->FindDouble("x");
  if (!maybe_x.has_value())
    return Status(kUnknownError, "x coordinate is missing in element location");

  std::optional<double> maybe_y = location_dict->FindDouble("y");
  if (!maybe_y.has_value())
    return Status(kUnknownError, "y coordinate is missing in element location");

  std::optional<double> maybe_height = size_dict->FindDouble("height");
  if (!maybe_height.has_value())
    return Status(kUnknownError, "height is missing in element size");

  std::optional<double> maybe_width = size_dict->FindDouble("width");
  if (!maybe_width.has_value())
    return Status(kUnknownError, "width is missing in element size");

  base::Value::Dict ret;
  ret.Set("x", maybe_x.value());
  ret.Set("y", maybe_y.value());
  ret.Set("width", maybe_width.value());
  ret.Set("height", maybe_height.value());
  *value = std::make_unique<base::Value>(std::move(ret));
  return Status(kOk);
}

Status ExecuteGetElementLocationOnceScrolledIntoView(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::Value::Dict& params,
    std::unique_ptr<base::Value>* value) {
  WebPoint offset(0, 0);
  WebPoint location;
  Status status = ScrollElementIntoView(
      session, web_view, element_id, &offset, &location);
  if (status.IsError())
    return status;
  *value = std::make_unique<base::Value>(CreateValueFrom(location));
  return Status(kOk);
}

Status ExecuteGetElementSize(Session* session,
                             WebView* web_view,
                             const std::string& element_id,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  base::Value::List args;
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
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value) {
  const std::string* attribute_name = params.FindString("name");
  if (!attribute_name)
    return Status(kInvalidArgument, "missing 'name'");

  // In legacy mode, use old behavior for backward compatibility.
  if (!session->w3c_compliant) {
    return GetElementAttribute(session, web_view, element_id, *attribute_name,
                               value);
  }

  base::Value::List args;
  args.Append(CreateElement(element_id));
  args.Append(*attribute_name);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      kBooleanAttributes.count(base::ToLowerASCII(*attribute_name))
          ? "(elem, attribute) => elem.hasAttribute(attribute) ? 'true' : null"
          : "(elem, attribute) => elem.getAttribute(attribute)",
      args, value);
}

Status ExecuteGetElementValueOfCSSProperty(
    Session* session,
    WebView* web_view,
    const std::string& element_id,
    const base::Value::Dict& params,
    std::unique_ptr<base::Value>* value) {
  bool is_xml = false;
  Status status = IsDocumentTypeXml(session, web_view, &is_xml);
  if (status.IsError())
    return status;

  if (is_xml) {
    *value = std::make_unique<base::Value>("");
  } else {
    const std::string* property_name = params.FindString("propertyName");
    if (!property_name)
      return Status(kInvalidArgument, "missing 'propertyName'");
    std::string property_value;
    status = GetElementEffectiveStyle(session, web_view, element_id,
                                      *property_name, &property_value);
    if (status.IsError())
      return status;
    *value = std::make_unique<base::Value>(property_value);
  }
  return Status(kOk);
}

Status ExecuteElementEquals(Session* session,
                            WebView* web_view,
                            const std::string& element_id,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value) {
  const std::string* other_element_id = params.FindString("other");
  if (!other_element_id)
    return Status(kInvalidArgument, "'other' must be a string");
  *value = std::make_unique<base::Value>(element_id == *other_element_id);
  return Status(kOk);
}

Status ExecuteElementScreenshot(Session* session,
                                WebView* web_view,
                                const std::string& element_id,
                                const base::Value::Dict& params,
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
  // Use window.pageXOffset and widnow.pageYOffset for scroll information,
  // should always return scroll amount regardless of doctype. The parentheses
  // around the JavaScript code below is needed because JavaScript syntax
  // doesn't allow a statement to start with an object literal.
  // document.documentElement.clientHeight and Width provide viewport height
  // and width to crop screenshot if necessary.
  std::unique_ptr<base::Value> browser_info;
  status = web_view->EvaluateScript(
      std::string(),
      "({x: window.pageXOffset,"
      "  y: window.pageYOffset,"
      "  height: document.documentElement.clientHeight,"
      "  width: document.documentElement.clientWidth,"
      "  device_pixel_ratio: window.devicePixelRatio})",
      false, &browser_info);
  if (status.IsError())
    return status;

  const base::Value::Dict& browser_info_dict = browser_info->GetDict();
  double scroll_left = browser_info_dict.FindDouble("x").value();
  double scroll_top = browser_info_dict.FindDouble("y").value();
  double viewport_height = browser_info_dict.FindDouble("height").value();
  double viewport_width = browser_info_dict.FindDouble("width").value();
  double device_pixel_ratio =
      browser_info_dict.FindDouble("device_pixel_ratio").value();

  if (!clip->is_dict())
    return Status(kUnknownError, "Element Rect is not a dictionary");

  base::Value::Dict screenshot_params;
  base::Value::Dict& clip_dict =
      screenshot_params
          .Set("clip", base::Value::FromUniquePtrValue(std::move(clip)))
          ->GetDict();
  // |clip_dict| already contains the right width and height of the target
  // element, but its x and y are relative to containing frame. We replace them
  // with the x and y relative to top-level document origin, as expected by
  // CaptureScreenshot.
  clip_dict.Set("x", location.x + scroll_left);
  clip_dict.Set("y", location.y + scroll_top);
  clip_dict.Set("scale", 1 / device_pixel_ratio);
  // Crop screenshot by viewport if element is larger than viewport
  clip_dict.Set("height", std::min(viewport_height - location.y,
                                   clip_dict.FindDouble("height").value()));
  clip_dict.Set("width", std::min(viewport_width - location.x,
                                  clip_dict.FindDouble("width").value()));

  std::string screenshot;
  status = web_view->CaptureScreenshot(&screenshot, screenshot_params);
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(screenshot);
  return Status(kOk);
}
