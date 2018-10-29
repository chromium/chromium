// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/window_commands.h"

#include <stddef.h>

#include <list>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/automation_extension.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/key_converter.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"

namespace {

// The error page URL was renamed in
// https://chromium-review.googlesource.com/c/580169, but because ChromeDriver
// needs to be backward-compatible with older versions of Chrome, it is
// necessary to compare against both the old and new error URL.
static const char kUnreachableWebDataURL[] = "chrome-error://chromewebdata/";
const char kDeprecatedUnreachableWebDataURL[] = "data:text/html,chromewebdata";

// Defaults to 20 years into the future when adding a cookie.
const double kDefaultCookieExpiryTime = 20*365*24*60*60;

// for pointer actions
enum class PointerActionType { NOT_INITIALIZED, PRESS, MOVE, RELEASE, IDLE };

Status GetMouseButton(const base::DictionaryValue& params,
                      MouseButton* button) {
  int button_num;
  if (!params.GetInteger("button", &button_num)) {
    button_num = 0;  // Default to left mouse button.
  } else if (button_num < 0 || button_num > 2) {
    return Status(kUnknownError,
                  base::StringPrintf("invalid button: %d", button_num));
  }
  *button = static_cast<MouseButton>(button_num);
  return Status(kOk);
}

Status GetUrl(WebView* web_view, const std::string& frame, std::string* url) {
  std::unique_ptr<base::Value> value;
  base::ListValue args;
  Status status = web_view->CallFunction(
      frame, "function() { return document.URL; }", args, &value);
  if (status.IsError())
    return status;
  if (!value->GetAsString(url))
    return Status(kUnknownError, "javascript failed to return the url");
  return Status(kOk);
}

struct Cookie {
  Cookie(const std::string& name,
         const std::string& value,
         const std::string& domain,
         const std::string& path,
         double expiry,
         bool http_only,
         bool secure,
         bool session)
      : name(name), value(value), domain(domain), path(path), expiry(expiry),
        http_only(http_only), secure(secure), session(session) {}

  std::string name;
  std::string value;
  std::string domain;
  std::string path;
  double expiry;
  bool http_only;
  bool secure;
  bool session;
};

std::unique_ptr<base::DictionaryValue> CreateDictionaryFrom(
    const Cookie& cookie) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("name", cookie.name);
  dict->SetString("value", cookie.value);
  if (!cookie.domain.empty())
    dict->SetString("domain", cookie.domain);
  if (!cookie.path.empty())
    dict->SetString("path", cookie.path);
  if (!cookie.session)
    dict->SetDouble("expiry", cookie.expiry);
  dict->SetBoolean("httpOnly", cookie.http_only);
  dict->SetBoolean("secure", cookie.secure);
  return dict;
}

Status GetVisibleCookies(WebView* web_view,
                         std::list<Cookie>* cookies) {
  std::string current_page_url;
  Status status = GetUrl(web_view, std::string(), &current_page_url);
  if (status.IsError())
    return status;
  std::unique_ptr<base::ListValue> internal_cookies;
  status = web_view->GetCookies(&internal_cookies, current_page_url);
  if (status.IsError())
    return status;
  std::list<Cookie> cookies_tmp;
  for (size_t i = 0; i < internal_cookies->GetSize(); ++i) {
    base::DictionaryValue* cookie_dict;
    if (!internal_cookies->GetDictionary(i, &cookie_dict))
      return Status(kUnknownError, "DevTools returns a non-dictionary cookie");

    std::string name;
    cookie_dict->GetString("name", &name);
    std::string value;
    cookie_dict->GetString("value", &value);
    std::string domain;
    cookie_dict->GetString("domain", &domain);
    std::string path;
    cookie_dict->GetString("path", &path);
    double expiry = 0;
    cookie_dict->GetDouble("expires", &expiry);
    if (expiry > 1e12)
      expiry /= 1000;  // Backwards compatibility ms -> sec.
    bool http_only = false;
    cookie_dict->GetBoolean("httpOnly", &http_only);
    bool session = false;
    cookie_dict->GetBoolean("session", &session);
    bool secure = false;
    cookie_dict->GetBoolean("secure", &secure);

    cookies_tmp.push_back(
        Cookie(name, value, domain, path, expiry, http_only, secure, session));
  }
  cookies->swap(cookies_tmp);
  return Status(kOk);
}

Status ScrollCoordinateInToView(
    Session* session, WebView* web_view, int x, int y, int* offset_x,
    int* offset_y) {
  std::unique_ptr<base::Value> value;
  base::ListValue args;
  args.AppendInteger(x);
  args.AppendInteger(y);
  Status status = web_view->CallFunction(
      std::string(),
      "function(x, y) {"
      "  if (x < window.pageXOffset ||"
      "      x >= window.pageXOffset + window.innerWidth ||"
      "      y < window.pageYOffset ||"
      "      y >= window.pageYOffset + window.innerHeight) {"
      "    window.scrollTo(x - window.innerWidth/2, y - window.innerHeight/2);"
      "  }"
      "  return {"
      "    view_x: Math.floor(window.pageXOffset),"
      "    view_y: Math.floor(window.pageYOffset),"
      "    view_width: Math.floor(window.innerWidth),"
      "    view_height: Math.floor(window.innerHeight)};"
      "}",
      args,
      &value);
  if (!status.IsOk())
    return status;
  base::DictionaryValue* view_attrib;
  value->GetAsDictionary(&view_attrib);
  int view_x, view_y, view_width, view_height;
  view_attrib->GetInteger("view_x", &view_x);
  view_attrib->GetInteger("view_y", &view_y);
  view_attrib->GetInteger("view_width", &view_width);
  view_attrib->GetInteger("view_height", &view_height);
  *offset_x = x - view_x;
  *offset_y = y - view_y;
  if (*offset_x < 0 || *offset_x >= view_width || *offset_y < 0 ||
      *offset_y >= view_height)
    return Status(kUnknownError, "Failed to scroll coordinate into view");
  return Status(kOk);
}

Status ExecuteTouchEvent(
    Session* session, WebView* web_view, TouchEventType type,
    const base::DictionaryValue& params) {
  int x, y;
  if (!params.GetInteger("x", &x))
    return Status(kUnknownError, "'x' must be an integer");
  if (!params.GetInteger("y", &y))
    return Status(kUnknownError, "'y' must be an integer");
  int relative_x = x;
  int relative_y = y;
  Status status = ScrollCoordinateInToView(
      session, web_view, x, y, &relative_x, &relative_y);
  if (!status.IsOk())
    return status;
  std::list<TouchEvent> events;
  events.push_back(
      TouchEvent(type, relative_x, relative_y));
  return web_view->DispatchTouchEvents(events);
}

}  // namespace

Status ExecuteWindowCommand(const WindowCommand& command,
                            Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  Timeout timeout;
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;

  JavaScriptDialogManager* dialog_manager =
      web_view->GetJavaScriptDialogManager();
  if (dialog_manager->IsDialogOpen()) {
    std::string alert_text;
    status = dialog_manager->GetDialogMessage(&alert_text);
    if (status.IsError())
      return status;

    // Close the dialog depending on the unexpectedalert behaviour set by user
    // before returning an error, so that subsequent commands do not fail.
    std::string prompt_behavior = session->unhandled_prompt_behavior;
    if (prompt_behavior == kAccept)
      status = dialog_manager->HandleDialog(true, session->prompt_text.get());
    else if (prompt_behavior == kDismiss)
      status = dialog_manager->HandleDialog(false, session->prompt_text.get());
    if (status.IsError())
      return status;

    return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
  }

  Status nav_status(kOk);
  for (int attempt = 0; attempt < 3; attempt++) {
    if (attempt == 2) {
      // Switch to main frame and retry command if subframe no longer exists.
      session->SwitchToTopFrame();
    }

    nav_status = web_view->WaitForPendingNavigations(
        session->GetCurrentFrameId(),
        Timeout(session->page_load_timeout, &timeout), true);
    if (nav_status.IsError())
      return nav_status;

    status = command.Run(session, web_view, params, value, &timeout);
    if (status.code() == kNoSuchExecutionContext || status.code() == kTimeout) {
      // If the command timed out, let WaitForPendingNavigations cancel
      // the navigation if there is one.
      continue;
    } else if (status.IsError()) {
      // If the command failed while a new page or frame started loading, retry
      // the command after the pending navigation has completed.
      bool is_pending = false;
      nav_status = web_view->IsPendingNavigation(session->GetCurrentFrameId(),
                                                 &timeout, &is_pending);
      if (nav_status.IsError())
        return nav_status;
      else if (is_pending)
        continue;
    }
    break;
  }

  nav_status = web_view->WaitForPendingNavigations(
      session->GetCurrentFrameId(),
      Timeout(session->page_load_timeout, &timeout), true);

  if (status.IsOk() && nav_status.IsError() &&
      nav_status.code() != kUnexpectedAlertOpen)
    return nav_status;
  if (status.code() == kUnexpectedAlertOpen)
    return Status(kOk);
  return status;
}

Status ExecuteGet(Session* session,
                  WebView* web_view,
                  const base::DictionaryValue& params,
                  std::unique_ptr<base::Value>* value,
                  Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  std::string url;
  if (!params.GetString("url", &url))
    return Status(kInvalidArgument, "'url' must be a string");
  Status status = web_view->Load(url, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteExecuteScript(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  std::string script;
  if (!params.GetString("script", &script))
    return Status(kInvalidArgument, "'script' must be a string");
  if (script == ":takeHeapSnapshot") {
    return web_view->TakeHeapSnapshot(value);
  } else if (script == ":startProfile") {
    return web_view->StartProfile();
  } else if (script == ":endProfile") {
    return web_view->EndProfile(value);
  } else {
    const base::ListValue* args;
    if (!params.GetList("args", &args))
      return Status(kInvalidArgument, "'args' must be a list");

    return web_view->CallFunction(session->GetCurrentFrameId(),
                                  "function(){" + script + "}", *args, value);
  }
}

Status ExecuteExecuteAsyncScript(Session* session,
                                 WebView* web_view,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value,
                                 Timeout* timeout) {
  std::string script;
  if (!params.GetString("script", &script))
    return Status(kInvalidArgument, "'script' must be a string");
  const base::ListValue* args;
  if (!params.GetList("args", &args))
    return Status(kInvalidArgument, "'args' must be a list");

  return web_view->CallUserAsyncFunction(
      session->GetCurrentFrameId(), "function(){" + script + "}", *args,
      session->script_timeout, value);
}

Status ExecuteSwitchToFrame(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const base::Value* id;
  if (!params.Get("id", &id))
    return Status(kUnknownError, "missing 'id'");

  if (id->is_none()) {
    session->SwitchToTopFrame();
    return Status(kOk);
  }

  std::string script;
  base::ListValue args;
  const base::DictionaryValue* id_dict;
  if (id->GetAsDictionary(&id_dict)) {
    std::string element_id;
    if (!id_dict->GetString(GetElementKey(), &element_id))
      return Status(kUnknownError, "missing 'ELEMENT'");
    bool is_displayed = false;
    Status status = IsElementDisplayed(
          session, web_view, element_id, true, &is_displayed);
    if (status.IsError())
      return status;
    script = "function(elem) { return elem; }";
    args.Append(id_dict->CreateDeepCopy());
  } else {
    script =
        "function(xpath) {"
        "  return document.evaluate(xpath, document, null, "
        "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
        "}";
    std::string xpath = "(/html/body//iframe|/html/frameset//frame)";
    std::string id_string;
    int id_int;
    if (id->GetAsString(&id_string)) {
      xpath += base::StringPrintf(
          "[@name=\"%s\" or @id=\"%s\"]", id_string.c_str(), id_string.c_str());
    } else if (id->GetAsInteger(&id_int)) {
      xpath += base::StringPrintf("[%d]", id_int + 1);
    } else {
      return Status(kUnknownError, "invalid 'id'");
    }
    args.AppendString(xpath);
  }
  std::string frame;
  Status status = web_view->GetFrameByFunction(
      session->GetCurrentFrameId(), script, args, &frame);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> result;
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), script, args, &result);
  if (status.IsError())
    return status;
  const base::DictionaryValue* element;
  if (!result->GetAsDictionary(&element))
    return Status(kUnknownError, "fail to locate the sub frame element");

  std::string chrome_driver_id = GenerateId();
  const char kSetFrameIdentifier[] =
      "function(frame, id) {"
      "  frame.setAttribute('cd_frame_id_', id);"
      "}";
  base::ListValue new_args;
  new_args.Append(element->CreateDeepCopy());
  new_args.AppendString(chrome_driver_id);
  result.reset(NULL);
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), kSetFrameIdentifier, new_args, &result);
  if (status.IsError())
    return status;
  session->SwitchToSubFrame(frame, chrome_driver_id);
  return Status(kOk);
}

Status ExecuteSwitchToParentFrame(Session* session,
                                  WebView* web_view,
                                  const base::DictionaryValue& params,
                                  std::unique_ptr<base::Value>* value,
                                  Timeout* timeout) {
  session->SwitchToParentFrame();
  return Status(kOk);
}

Status ExecuteGetTitle(Session* session,
                       WebView* web_view,
                       const base::DictionaryValue& params,
                       std::unique_ptr<base::Value>* value,
                       Timeout* timeout) {
  const char kGetTitleScript[] = "function() {  return document.title;}";
  base::ListValue args;
  return web_view->CallFunction(std::string(), kGetTitleScript, args, value);
}

Status ExecuteGetPageSource(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const char kGetPageSource[] =
      "function() {"
      "  return new XMLSerializer().serializeToString(document);"
      "}";
  base::ListValue args;
  return web_view->CallFunction(
      session->GetCurrentFrameId(), kGetPageSource, args, value);
}

Status ExecuteFindElement(int interval_ms,
                          Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  return FindElement(interval_ms, true, NULL, session, web_view, params, value);
}

Status ExecuteFindElements(int interval_ms,
                           Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return FindElement(
      interval_ms, false, NULL, session, web_view, params, value);
}

Status ExecuteGetCurrentUrl(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  std::string url;
  Status status = GetUrl(web_view, std::string(), &url);
  if (status.IsError())
    return status;
  if (url == kUnreachableWebDataURL ||
      url == kDeprecatedUnreachableWebDataURL) {
    status = web_view->GetUrl(&url);
    if (status.IsError())
      return status;
  }
  value->reset(new base::Value(url));
  return Status(kOk);
}

Status ExecuteGoBack(Session* session,
                     WebView* web_view,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value,
                     Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->TraverseHistory(-1, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteGoForward(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->TraverseHistory(1, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteRefresh(Session* session,
                      WebView* web_view,
                      const base::DictionaryValue& params,
                      std::unique_ptr<base::Value>* value,
                      Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->Reload(timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteFreeze(Session* session,
                     WebView* web_view,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value,
                     Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->Freeze(timeout);
  return status;
}

Status ExecuteResume(Session* session,
                     WebView* web_view,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value,
                     Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->Resume(timeout);
  if (status.IsError())
    return status;
  return Status(kOk);
}

Status ExecuteMouseMoveTo(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  std::string element_id;
  bool has_element = params.GetString("element", &element_id);
  int x_offset = 0;
  int y_offset = 0;
  bool has_offset = params.GetInteger("xoffset", &x_offset) &&
      params.GetInteger("yoffset", &y_offset);
  if (!has_element && !has_offset)
    return Status(kUnknownError, "at least an element or offset should be set");

  WebPoint location;
  if (has_element) {
    WebPoint offset(x_offset, y_offset);
    Status status = ScrollElementIntoView(session, web_view, element_id,
        has_offset ? &offset : nullptr, &location);
    if (status.IsError())
      return status;
  } else {
    location = session->mouse_position;
    if (has_offset)
      location.Offset(x_offset, y_offset);
  }

  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kMovedMouseEventType, session->pressed_mouse_button,
                 location.x, location.y, session->sticky_modifiers, 0));
  Status status =
      web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
  if (status.IsOk())
    session->mouse_position = location;
  return status;
}

Status ExecuteMouseClick(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  session->pressed_mouse_button = kNoneMouseButton;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteMouseButtonDown(Session* session,
                              WebView* web_view,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  session->pressed_mouse_button = button;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteMouseButtonUp(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 1));
  session->pressed_mouse_button = kNoneMouseButton;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteMouseDoubleClick(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  MouseButton button;
  Status status = GetMouseButton(params, &button);
  if (status.IsError())
    return status;
  std::list<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 2));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button,
                 session->mouse_position.x, session->mouse_position.y,
                 session->sticky_modifiers, 2));
  session->pressed_mouse_button = kNoneMouseButton;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId());
}

Status ExecuteTouchDown(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchStart, params);
}

Status ExecuteTouchUp(Session* session,
                      WebView* web_view,
                      const base::DictionaryValue& params,
                      std::unique_ptr<base::Value>* value,
                      Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchEnd, params);
}

Status ExecuteTouchMove(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchMove, params);
}

Status ExecuteTouchScroll(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  WebPoint location = session->mouse_position;
  std::string element;
  if (params.GetString("element", &element)) {
    Status status = GetElementClickableLocation(
        session, web_view, element, &location);
    if (status.IsError())
      return status;
  }
  int xoffset;
  if (!params.GetInteger("xoffset", &xoffset))
    return Status(kUnknownError, "'xoffset' must be an integer");
  int yoffset;
  if (!params.GetInteger("yoffset", &yoffset))
    return Status(kUnknownError, "'yoffset' must be an integer");
  return web_view->SynthesizeScrollGesture(
      location.x, location.y, xoffset, yoffset);
}

Status ExecuteTouchPinch(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  WebPoint location;
  if (!params.GetInteger("x", &location.x))
    return Status(kUnknownError, "'x' must be an integer");
  if (!params.GetInteger("y", &location.y))
    return Status(kUnknownError, "'y' must be an integer");
  double scale_factor;
  if (!params.GetDouble("scale", &scale_factor))
    return Status(kUnknownError, "'scale' must be an integer");
  return web_view->SynthesizePinchGesture(location.x, location.y, scale_factor);
}

Status ProcessInputActionSequence(Session* session,
                                  const base::DictionaryValue* action_sequence,
                                  std::unique_ptr<base::ListValue>* result) {
  std::string id;
  std::string type;
  const base::DictionaryValue* source;
  const base::DictionaryValue* parameters;
  std::string pointer_type = "mouse";

  if (!action_sequence->GetString("type", &type) ||
      ((type != "key") && (type != "pointer") && (type != "none"))) {
    return Status(
        kInvalidArgument,
        "'type' must be one of the strings 'key', 'pointer' or 'none'");
  }

  if (!action_sequence->GetString("id", &id))
    return Status(kInvalidArgument, "'id' must be a string");

  if (type == "pointer") {
    if (action_sequence->GetDictionary("parameters", &parameters)) {
      // error check arguments
      if (parameters->GetString("pointerType", &pointer_type) &&
          (pointer_type != "mouse" && pointer_type != "pen" &&
           pointer_type != "touch"))
        return Status(kInvalidArgument,
                      "'pointerType' must be one of mouse, pen or touch");
    }
  }

  bool found = false;
  for (size_t i = 0; i < session->active_input_sources->GetSize(); i++) {
    session->active_input_sources->GetDictionary(i, &source);
    DCHECK(source);

    std::string source_id;
    source->GetString("id", &source_id);
    if (source_id == id) {
      found = true;
      if (type == "pointer") {
        std::string source_pointer_type;
        if (!source->GetString("pointerType", &source_pointer_type) ||
            pointer_type != source_pointer_type) {
          return Status(kInvalidArgument,
                        "'pointerType' must be a string that matches sources "
                        "pointer type");
        }
      }
      std::string source_type;
      source->GetString("type", &source_type);
      if (source_type != type) {
        return Status(kInvalidArgument,
                      "input state with same id has a different type");
      }
      break;
    }
  }

  // if we found no matching active input source
  base::DictionaryValue tmp_source;
  if (!found) {
    // create input source
    tmp_source.SetString("id", id);
    tmp_source.SetString("type", type);
    if (type == "pointer") {
      tmp_source.SetString("pointerType", pointer_type);
    }

    session->active_input_sources->Append(
        std::make_unique<base::DictionaryValue>(std::move(tmp_source)));

    base::DictionaryValue tmp_state;
    if (type == "key") {
      std::unique_ptr<base::ListValue> pressed(new base::ListValue);
      bool alt = false;
      bool shift = false;
      bool ctrl = false;
      bool meta = false;

      tmp_state.SetList("pressed", std::move(pressed));
      tmp_state.SetBoolean("alt", alt);
      tmp_state.SetBoolean("shift", shift);
      tmp_state.SetBoolean("ctrl", ctrl);
      tmp_state.SetBoolean("meta", meta);
    } else if (type == "pointer") {
      std::unique_ptr<base::ListValue> pressed(new base::ListValue);
      int x = 0;
      int y = 0;

      tmp_state.SetList("pressed", std::move(pressed));
      tmp_state.SetString("subtype", pointer_type);

      tmp_state.SetInteger("x", x);
      tmp_state.SetInteger("y", y);
    }
    session->input_state_table->SetDictionary(
        id, std::make_unique<base::DictionaryValue>(std::move(tmp_state)));
  }

  const base::ListValue* actions;
  if (!action_sequence->GetList("actions", &actions)) {
    return Status(kInvalidArgument, "actions must be an array");
  }

  std::unique_ptr<base::ListValue> ret(new base::ListValue);
  for (size_t i = 0; i < actions->GetSize(); i++) {
    std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());
    const base::DictionaryValue* action_item;
    if (!actions->GetDictionary(i, &action_item))
      return Status(
          kInvalidArgument,
          "each argument in the action sequence must be a dictionary");

    if (type == "none") {
      // process null action
      std::string subtype;
      if (!action_item->GetString("type", &subtype) || subtype != "pause")
        return Status(kInvalidArgument,
                      "type of action must be the  string 'pause'");

      action->SetString("id", id);
      action->SetString("type", "none");
      action->SetString("subtype", subtype);

      int duration;
      if (action_item->GetInteger("duration", &duration)) {
        if (duration < 0)
          return Status(kInvalidArgument,
                        "duration must be a non-negative int");
        action->SetInteger("duration", duration);
      }
    } else if (type == "key") {
      // process key action
      std::string subtype;
      if (!action_item->GetString("type", &subtype) ||
          (subtype != "keyUp" && subtype != "keyDown" && subtype != "pause"))
        return Status(
            kInvalidArgument,
            "type of action must be the string 'keyUp', 'keyDown' or 'pause'");

      action->SetString("id", id);
      action->SetString("type", "key");
      action->SetString("subtype", subtype);

      if (subtype == "pause") {
        int duration;
        if (action_item->GetInteger("duration", &duration)) {
          if (duration < 0)
            return Status(kInvalidArgument,
                          "duration must be a non-negative int");
          action->SetInteger("duration", duration);
        }
      }
      std::string key;
      // TODO: check if key is a single unicode code point
      if (!action_item->GetString("value", &key)) {
        return Status(kInvalidArgument,
                      "'value' must be a single unicode point");
      }
      action->SetString("value", key);
    } else if (type == "pointer") {
      std::string subtype;
      if (!action_item->GetString("type", &subtype) ||
          (subtype != "pointerUp" && subtype != "pointerDown" &&
           subtype != "pointerMove" && subtype != "pointerCancel" &&
           subtype != "pause"))
        return Status(kInvalidArgument,
                      "type of action must be the string 'pointerUp', "
                      "'pointerDown', 'pointerMove' or 'pause'");

      action->SetString("id", id);
      action->SetString("type", "pointer");
      action->SetString("subtype", subtype);

      if (subtype == "pause") {
        int duration;
        if (action_item->GetInteger("duration", &duration)) {
          if (duration < 0)
            return Status(kInvalidArgument,
                          "duration must be a non-negative int");
          action->SetInteger("duration", duration);
        }
      }

      action->SetString("pointerType", pointer_type);
      if (subtype == "pointerUp" || subtype == "pointerDown") {
        int button;
        if (!action_item->GetInteger("button", &button) || button < 0)
          return Status(kInvalidArgument,
                        "'button' must be a non-negative int");
        action->SetInteger("button", button);
        if (subtype == "pointerDown") {
          int x;
          if (!action_item->GetInteger("x", &x))
            return Status(kInvalidArgument, "'x' must be an integer");
          int y;
          if (!action_item->GetInteger("y", &y))
            return Status(kInvalidArgument, "'y' must be an integer");

          action->SetInteger("x", x);
          action->SetInteger("y", y);
        }
      } else {
        // pointerMove
        int duration;
        if (!action_item->GetInteger("duration", &duration) || duration < 0)
          return Status(kInvalidArgument,
                        "'duration' must be a non-negative int");

        std::string origin;
        if (!action_item->GetString("origin", &origin))
          origin = "viewport";
        if (origin != "viewport" && origin != "pointer")
          return Status(kInvalidArgument, "'origin' must be a string");

        action->SetString("origin", origin);

        int x;
        if (!action_item->GetInteger("x", &x))
          return Status(kInvalidArgument, "'x' must be an integer");
        int y;
        if (!action_item->GetInteger("y", &y))
          return Status(kInvalidArgument, "'y' must be an integer");

        action->SetInteger("x", x);
        action->SetInteger("y", y);
      }
    }
    ret->Append(std::move(action));
  }
  *result = std::move(ret);
  return Status(kOk);
}

Status ExecutePerformActions(Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  // TODO(kereliuk): check if the current browsing context is still open
  // or if this error check is handled elsewhere

  // TODO(kereliuk): handle prompts

  // extract action sequence
  const base::ListValue* actions;
  if (!params.GetList("actions", &actions))
    return Status(kInvalidArgument, "'actions' must be an array");

  // the processed actions
  base::ListValue actions_by_tick;
  // the type of each action list in actions_by_tick
  std::list<std::string> action_list_types;

  for (size_t i = 0; i < actions->GetSize(); i++) {
    std::unique_ptr<base::ListValue> input_source_actions(
        new base::ListValue());
    // proccess input action sequence
    const base::DictionaryValue* action_sequence;
    if (!actions->GetDictionary(i, &action_sequence))
      return Status(kInvalidArgument, "each argument must be a dictionary");

    std::string type;
    if (!action_sequence->GetString("type", &type) ||
        ((type != "key") && (type != "pointer") && (type != "none"))) {
      return Status(
          kInvalidArgument,
          "'type' must be one of the strings 'key', 'pointer' or 'none'");
    }
    action_list_types.push_back(type);

    Status status = ProcessInputActionSequence(session, action_sequence,
                                               &input_source_actions);
    if (status.IsError())
      return Status(kInvalidArgument, status);

    actions_by_tick.Append(std::move(input_source_actions));
  }

  for (size_t i = 0; i < actions_by_tick.GetSize(); i++) {
    // compute duration
    int max_duration = 0;
    int duration;
    base::ListValue* action_sequence;
    actions_by_tick.GetList(i, &action_sequence);
    DCHECK(action_sequence);
    for (size_t j = 0; j < action_sequence->GetSize(); j++) {
      base::DictionaryValue* action;
      if (!action_sequence->GetDictionary(i, &action))
        return Status(kInvalidArgument, "each argument must be a dictionary");
      if (action->GetInteger("duration", &duration) &&
          duration > max_duration) {
        max_duration = duration;
      }
    }

    // get the type of the actions so we can dispatch all at once for that type
    std::string type = action_list_types.back();
    action_list_types.pop_back();

    // pause only

    // key actions
    if (type == "key") {
      KeyEventBuilder builder;
      std::list<KeyEvent> key_events;
      for (size_t j = 0; j < action_sequence->GetSize(); j++) {
        base::DictionaryValue* action;
        if (!action_sequence->GetDictionary(j, &action))
          return Status(kInvalidArgument, "each argument must be a dictionary");
        std::string subtype;
        if (!action->GetString("subtype", &subtype))
          return Status(kInvalidArgument, "'type' must be a string");

        std::string id;
        if (!action->GetString("id", &id))
          return Status(kInvalidArgument, "id");

        if (subtype == "pause") {
          // TODO: handle this
        } else {
          base::DictionaryValue dispatch_params;
          base::string16 raw_key;

          if (!action->GetString("value", &raw_key))
            return Status(kInvalidArgument, "value");

          base::char16 key = raw_key[0];
          // TODO: understand necessary_modifiers
          int necessary_modifiers = 0;
          ui::KeyboardCode key_code = ui::VKEY_UNKNOWN;
          std::string error_msg;
          ConvertCharToKeyCode(key, &key_code, &necessary_modifiers,
                               &error_msg);
          if (!error_msg.empty())
            return Status(kUnknownError, error_msg);

          if (subtype == "keyDown")
            key_events.push_back(builder.SetType(kKeyDownEventType)
                                     ->SetText(base::UTF16ToUTF8(raw_key),
                                               base::UTF16ToUTF8(raw_key))
                                     ->SetKeyCode(key_code)
                                     ->SetModifiers(0)
                                     ->Build());
          else if (subtype == "keyUp")
            key_events.push_back(builder.SetType(kKeyUpEventType)
                                     ->SetText(base::UTF16ToUTF8(raw_key),
                                               base::UTF16ToUTF8(raw_key))
                                     ->SetKeyCode(key_code)
                                     ->SetModifiers(0)
                                     ->Build());
        }
      }
      Status status = web_view->DispatchKeyEvents(key_events);
      if (status.IsError())
        return status;
    } else if (type == "pointer") {
      // TODO:implement this
    }
  }
  return Status(kOk);
}

Status ExecuteSendCommand(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  std::string cmd;
  if (!params.GetString("cmd", &cmd)) {
    return Status(kUnknownError, "command not passed");
  }
  const base::DictionaryValue* cmdParams;
  if (!params.GetDictionary("params", &cmdParams)) {
    return Status(kUnknownError, "params not passed");
  }
  return web_view->SendCommand(cmd, *cmdParams);
}

Status ExecuteSendCommandAndGetResult(Session* session,
                                      WebView* web_view,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  std::string cmd;
  if (!params.GetString("cmd", &cmd)) {
    return Status(kUnknownError, "command not passed");
  }
  const base::DictionaryValue* cmdParams;
  if (!params.GetDictionary("params", &cmdParams)) {
    return Status(kUnknownError, "params not passed");
  }
  return web_view->SendCommandAndGetResult(cmd, *cmdParams, value);
}

Status ExecuteGetActiveElement(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  return GetActiveElement(session, web_view, value);
}

Status ExecuteSendKeysToActiveElement(Session* session,
                                      WebView* web_view,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  const base::ListValue* key_list;
  if (!params.GetList("value", &key_list))
    return Status(kUnknownError, "'value' must be a list");
  return SendKeysOnWindow(
      web_view, key_list, false, &session->sticky_modifiers);
}

Status ExecuteGetAppCacheStatus(Session* session,
                                WebView* web_view,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      "applicationCache.status",
      value);
}

Status ExecuteIsBrowserOnline(Session* session,
                              WebView* web_view,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      "navigator.onLine",
      value);
}

Status ExecuteGetStorageItem(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kUnknownError, "'key' must be a string");
  base::ListValue args;
  args.AppendString(key);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key) { return %s[key]; }", storage),
      args,
      value);
}

Status ExecuteGetStorageKeys(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  const char script[] =
      "var keys = [];"
      "var storage = %s;"
      "for (var i = 0; i < storage.length; i++) {"
      "  keys.push(storage.key(i));"
      "}"
      "keys";
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      base::StringPrintf(script, storage),
      value);
}

Status ExecuteSetStorageItem(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kUnknownError, "'key' must be a string");
  std::string storage_value;
  if (!params.GetString("value", &storage_value))
    return Status(kUnknownError, "'value' must be a string");
  base::ListValue args;
  args.AppendString(key);
  args.AppendString(storage_value);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key, value) { %s[key] = value; }", storage),
      args,
      value);
}

Status ExecuteRemoveStorageItem(const char* storage,
                                Session* session,
                                WebView* web_view,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kUnknownError, "'key' must be a string");
  base::ListValue args;
  args.AppendString(key);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key) { %s.removeItem(key) }", storage),
      args,
      value);
}

Status ExecuteClearStorage(const char* storage,
                           Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      base::StringPrintf("%s.clear()", storage),
      value);
}

Status ExecuteGetStorageSize(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  return web_view->EvaluateScript(
      session->GetCurrentFrameId(),
      base::StringPrintf("%s.length", storage),
      value);
}

Status ExecuteScreenshot(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  Status status = session->chrome->ActivateWebView(web_view->GetId());
  if (status.IsError())
    return status;

  std::string screenshot;
  ChromeDesktopImpl* desktop = NULL;
  status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsOk() && !session->force_devtools_screenshot) {
    AutomationExtension* extension = NULL;
    status = desktop->GetAutomationExtension(&extension,
                                             session->w3c_compliant);
    if (status.IsError())
      return status;
    status = extension->CaptureScreenshot(&screenshot);
  } else {
    std::unique_ptr<base::DictionaryValue> screenshot_params(
        const base::DictionaryValue&);
  status = web_view->CaptureScreenshot(&screenshot, base::DictionaryValue());
  }
  if (status.IsError()) {
    LOG(WARNING) << "screenshot failed, retrying";
    std::unique_ptr<base::DictionaryValue> screenshot_params(
        new base::DictionaryValue);
    status = web_view->CaptureScreenshot(&screenshot, base::DictionaryValue());
  }
  if (status.IsError())
    return status;

  value->reset(new base::Value(screenshot));
  return Status(kOk);
}

Status ExecuteGetCookies(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(web_view, &cookies);
  if (status.IsError())
    return status;
  std::unique_ptr<base::ListValue> cookie_list(new base::ListValue());
  for (std::list<Cookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    cookie_list->Append(CreateDictionaryFrom(*it));
  }
  *value = std::move(cookie_list);
  return Status(kOk);
}

Status ExecuteGetNamedCookie(Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "missing 'cookie name'");

  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(web_view, &cookies);
  if (status.IsError())
    return status;

  for (std::list<Cookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (name == it->name) {
      value->reset(CreateDictionaryFrom(*it)->DeepCopy());
      return Status(kOk);
    }
  }
  return Status(kNoSuchCookie);
}

Status ExecuteAddCookie(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  const base::DictionaryValue* cookie;
  if (!params.GetDictionary("cookie", &cookie))
    return Status(kUnknownError, "missing 'cookie'");
  std::string name;
  std::string cookie_value;
  if (!cookie->GetString("name", &name))
    return Status(kInvalidArgument, "missing 'name'");
  if (!cookie->GetString("value", &cookie_value))
    return Status(kInvalidArgument, "missing 'value'");
  std::string url;
  Status status = GetUrl(web_view, session->GetCurrentFrameId(), &url);
  if (status.IsError())
    return status;
  std::string domain;
  cookie->GetString("domain", &domain);
  std::string path("/");
  cookie->GetString("path", &path);
  bool secure = false;
  cookie->GetBoolean("secure", &secure);
  bool httpOnly = false;
  cookie->GetBoolean("httpOnly", &httpOnly);
  double expiry;
  if (!cookie->GetDouble("expiry", &expiry))
    expiry = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() +
              kDefaultCookieExpiryTime;
  return web_view->AddCookie(name, url, cookie_value, domain, path,
      secure, httpOnly, expiry);
}

Status ExecuteDeleteCookie(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "missing 'name'");
  base::DictionaryValue params_url;
  std::unique_ptr<base::Value> value_url;
  std::string url;
  Status status = GetUrl(web_view, session->GetCurrentFrameId(), &url);
  if (status.IsError())
    return status;

  std::list<Cookie> cookies;
  status = GetVisibleCookies(web_view, &cookies);
  if (status.IsError())
    return status;

  for (std::list<Cookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (name == it->name) {
      status = web_view->DeleteCookie(it->name, url, it->domain, it->path);
      if (status.IsError())
        return status;
    }
  }
  return Status(kOk);
}

Status ExecuteDeleteAllCookies(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(web_view, &cookies);
  if (status.IsError())
    return status;

  if (!cookies.empty()) {
    base::DictionaryValue params_url;
    std::unique_ptr<base::Value> value_url;
    std::string url;
    status = GetUrl(web_view, session->GetCurrentFrameId(), &url);
    if (status.IsError())
      return status;
    for (std::list<Cookie>::const_iterator it = cookies.begin();
         it != cookies.end(); ++it) {
      status = web_view->DeleteCookie(it->name, url, it->domain, it->path);
      if (status.IsError())
        return status;
    }
  }

  return Status(kOk);
}

Status ExecuteSetLocation(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  const base::DictionaryValue* location = NULL;
  Geoposition geoposition;
  if (!params.GetDictionary("location", &location) ||
      !location->GetDouble("latitude", &geoposition.latitude) ||
      !location->GetDouble("longitude", &geoposition.longitude))
    return Status(kUnknownError, "missing or invalid 'location'");
  if (location->HasKey("accuracy") &&
      !location->GetDouble("accuracy", &geoposition.accuracy)) {
    return Status(kUnknownError, "invalid 'accuracy'");
  } else {
    // |accuracy| is not part of the WebDriver spec yet, so if it is not given
    // default to 100 meters accuracy.
    geoposition.accuracy = 100;
  }

  Status status = web_view->OverrideGeolocation(geoposition);
  if (status.IsOk())
    session->overridden_geoposition.reset(new Geoposition(geoposition));
  return status;
}

Status ExecuteSetNetworkConditions(Session* session,
                                   WebView* web_view,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value,
                                   Timeout* timeout) {
  std::string network_name;
  const base::DictionaryValue* conditions = NULL;
  std::unique_ptr<NetworkConditions> network_conditions(
      new NetworkConditions());
  if (params.GetString("network_name", &network_name)) {
    // Get conditions from preset list.
    Status status = FindPresetNetwork(network_name, network_conditions.get());
    if (status.IsError())
      return status;
  } else if (params.GetDictionary("network_conditions", &conditions)) {
    // |latency| is required.
    if (!conditions->GetDouble("latency", &network_conditions->latency))
      return Status(kUnknownError,
                    "invalid 'network_conditions' is missing 'latency'");

    // Either |throughput| or the pair |download_throughput| and
    // |upload_throughput| is required.
    if (conditions->HasKey("throughput")) {
      if (!conditions->GetDouble("throughput",
                                 &network_conditions->download_throughput))
        return Status(kUnknownError, "invalid 'throughput'");
      conditions->GetDouble("throughput",
                            &network_conditions->upload_throughput);
    } else if (conditions->HasKey("download_throughput") &&
               conditions->HasKey("upload_throughput")) {
      if (!conditions->GetDouble("download_throughput",
                                 &network_conditions->download_throughput) ||
          !conditions->GetDouble("upload_throughput",
                                 &network_conditions->upload_throughput))
        return Status(kUnknownError,
                      "invalid 'download_throughput' or 'upload_throughput'");
    } else {
      return Status(kUnknownError,
                    "invalid 'network_conditions' is missing 'throughput' or "
                    "'download_throughput'/'upload_throughput' pair");
    }

    // |offline| is optional.
    if (conditions->HasKey("offline")) {
      if (!conditions->GetBoolean("offline", &network_conditions->offline))
        return Status(kUnknownError, "invalid 'offline'");
    } else {
      network_conditions->offline = false;
    }
  } else {
    return Status(kUnknownError,
                  "either 'network_conditions' or 'network_name' must be "
                  "supplied");
  }

  session->overridden_network_conditions.reset(
      network_conditions.release());
  return web_view->OverrideNetworkConditions(
      *session->overridden_network_conditions);
}

Status ExecuteDeleteNetworkConditions(Session* session,
                                      WebView* web_view,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  // Chrome does not have any command to stop overriding network conditions, so
  // we just override the network conditions with the "No throttling" preset.
  NetworkConditions network_conditions;
  // Get conditions from preset list.
  Status status = FindPresetNetwork("No throttling", &network_conditions);
  if (status.IsError())
    return status;

  status = web_view->OverrideNetworkConditions(network_conditions);
  if (status.IsError())
    return status;

  // After we've successfully overridden the network conditions with
  // "No throttling", we can delete them from |session|.
  session->overridden_network_conditions.reset();
  return status;
}

Status ExecuteTakeHeapSnapshot(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  return web_view->TakeHeapSnapshot(value);
}
