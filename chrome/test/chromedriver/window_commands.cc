// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/window_commands.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/js.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/key_converter.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"
#include "chrome/test/chromedriver/net/command_id.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "ui/gfx/geometry/point.h"
#include "url/url_util.h"

namespace {

// The error page URL was renamed in
// https://chromium-review.googlesource.com/c/580169, but because ChromeDriver
// needs to be backward-compatible with older versions of Chrome, it is
// necessary to compare against both the old and new error URL.
static const char kUnreachableWebDataURL[] = "chrome-error://chromewebdata/";
const char kDeprecatedUnreachableWebDataURL[] = "data:text/html,chromewebdata";

// Match to content/browser/devtools/devTools_session const of same name
const char kTargetClosedMessage[] = "Inspected target navigated or closed";

// TODO(crbug.com/chromedriver/2596): Remove when we stop supporting legacy
// protocol.
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
    return Status(kInvalidArgument,
                  base::StringPrintf("invalid button: %d", button_num));
  }
  *button = static_cast<MouseButton>(button_num);
  return Status(kOk);
}

Status IntToStringButton(int button, std::string& out) {
  if (button == 0) {
    out = "left";
  } else if (button == 1) {
    out = "middle";
  } else if (button == 2) {
    out = "right";
  } else if (button == 3) {
    out = "back";
  } else if (button == 4) {
    out = "forward";
  } else {
    return Status(kInvalidArgument,
                  "'button' must be an integer between 0 and 4 inclusive");
  }
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

MouseEventType StringToMouseEventType(std::string action_type) {
  if (action_type == "pointerDown")
    return kPressedMouseEventType;
  else if (action_type == "pointerUp")
    return kReleasedMouseEventType;
  else if (action_type == "pointerMove")
    return kMovedMouseEventType;
  else if (action_type == "scroll")
    return kWheelMouseEventType;
  else if (action_type == "pause")
    return kPauseMouseEventType;
  else
    return kPressedMouseEventType;
}

MouseButton StringToMouseButton(std::string button_type) {
  if (button_type == "left")
    return kLeftMouseButton;
  else if (button_type == "middle")
    return kMiddleMouseButton;
  else if (button_type == "right")
    return kRightMouseButton;
  else if (button_type == "back")
    return kBackMouseButton;
  else if (button_type == "forward")
    return kForwardMouseButton;
  else
    return kNoneMouseButton;
}

TouchEventType StringToTouchEventType(std::string action_type) {
  if (action_type == "pointerDown")
    return kTouchStart;
  else if (action_type == "pointerUp")
    return kTouchEnd;
  else if (action_type == "pointerMove")
    return kTouchMove;
  else if (action_type == "pointerCancel")
    return kTouchCancel;
  else if (action_type == "pause")
    return kPause;
  else
    return kTouchStart;
}

int StringToModifierMouseButton(std::string button_type) {
  if (button_type == "left")
    return 1;
  else if (button_type == "right")
    return 2;
  else if (button_type == "middle")
    return 4;
  else if (button_type == "back")
    return 8;
  else if (button_type == "forward")
    return 16;
  else
    return 0;
}

int MouseButtonToButtons(MouseButton button) {
  switch (button) {
    case kLeftMouseButton:
      return 1;
    case kRightMouseButton:
      return 2;
    case kMiddleMouseButton:
      return 4;
    case kBackMouseButton:
      return 8;
    case kForwardMouseButton:
      return 16;
    default:
      return 0;
  }
}

int KeyToKeyModifiers(std::string key) {
  if (key == "Shift") {
    return kShiftKeyModifierMask;
  } else if (key == "Control") {
    return kControlKeyModifierMask;
  } else if (key == "Alt") {
    return kAltKeyModifierMask;
  } else if (key == "Meta") {
    return kMetaKeyModifierMask;
  } else {
    return 0;
  }
}

PointerType StringToPointerType(std::string pointer_type) {
  CHECK(pointer_type == "pen" || pointer_type == "mouse");
  if (pointer_type == "pen")
    return kPen;
  else
    return kMouse;
}

struct Cookie {
  Cookie(const std::string& name,
         const std::string& value,
         const std::string& domain,
         const std::string& path,
         const std::string& samesite,
         int64_t expiry,
         bool http_only,
         bool secure,
         bool session)
      : name(name),
        value(value),
        domain(domain),
        path(path),
        samesite(samesite),
        expiry(expiry),
        http_only(http_only),
        secure(secure),
        session(session) {}

  std::string name;
  std::string value;
  std::string domain;
  std::string path;
  std::string samesite;
  int64_t expiry;
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
    SetSafeInt(dict.get(), "expiry", cookie.expiry);
  dict->SetBoolean("httpOnly", cookie.http_only);
  dict->SetBoolean("secure", cookie.secure);
  if (!cookie.samesite.empty())
    dict->SetString("sameSite", cookie.samesite);
  return dict;
}

Status GetVisibleCookies(Session* session,
                         WebView* web_view,
                         std::list<Cookie>* cookies) {
  std::string current_page_url;
  Status status =
      GetUrl(web_view, session->GetCurrentFrameId(), &current_page_url);
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
    std::string samesite;
    GetOptionalString(cookie_dict, "sameSite", &samesite);
    int64_t expiry = 0;
    double temp_double;
    if (cookie_dict->GetDouble("expires", &temp_double)) {
      // Truncate & convert the value to an integer as required by W3C spec.
      int64_t temp_int64 = static_cast<int64_t>(temp_double);
      if (!(temp_int64 >= (1ll << 53) || temp_int64 <= -(1ll << 53)))
        expiry = temp_int64;
    }
    bool http_only = false;
    cookie_dict->GetBoolean("httpOnly", &http_only);
    bool session = false;
    cookie_dict->GetBoolean("session", &session);
    bool secure = false;
    cookie_dict->GetBoolean("secure", &secure);

    cookies_tmp.push_back(Cookie(name, value, domain, path, samesite, expiry,
                                 http_only, secure, session));
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
    return Status(kInvalidArgument, "'x' must be an integer");
  if (!params.GetInteger("y", &y))
    return Status(kInvalidArgument, "'y' must be an integer");
  int relative_x = x;
  int relative_y = y;
  Status status = ScrollCoordinateInToView(
      session, web_view, x, y, &relative_x, &relative_y);
  if (!status.IsOk())
    return status;
  std::vector<TouchEvent> events;
  events.push_back(
      TouchEvent(type, relative_x, relative_y));
  return web_view->DispatchTouchEvents(events, false);
}

Status WindowViewportSize(Session* session,
                          WebView* web_view,
                          int* innerWidth,
                          int* innerHeight) {
  std::unique_ptr<base::Value> value;
  base::ListValue args;
  Status status =
      web_view->CallFunction(std::string(),
                             "function() {"
                             "  return {"
                             "    view_width: Math.floor(window.innerWidth),"
                             "    view_height: Math.floor(window.innerHeight)};"
                             "}",
                             args, &value);
  if (!status.IsOk())
    return status;
  base::DictionaryValue* view_attrib;
  value->GetAsDictionary(&view_attrib);
  view_attrib->GetInteger("view_width", innerWidth);
  view_attrib->GetInteger("view_height", innerHeight);
  return Status(kOk);
}

Status ProcessPauseAction(const base::DictionaryValue* action_item,
                          base::DictionaryValue* action) {
  int duration = 0;
  bool has_value = false;
  if (!GetOptionalInt(action_item, "duration", &duration, &has_value) ||
      duration < 0)
    return Status(kInvalidArgument, "'duration' must be a non-negative int");
  if (has_value)
    action->SetInteger("duration", duration);
  return Status(kOk);
}

Status ElementInViewCenter(Session* session,
                           WebView* web_view,
                           std::string element_id,
                           int* center_x,
                           int* center_y) {
  WebPoint center_location;
  Status status = GetElementLocationInViewCenter(session, web_view, element_id,
                                                 &center_location);
  if (status.IsError())
    return status;

  *center_x = center_location.x;
  *center_y = center_location.y;
  return Status(kOk);
}

int GetMouseClickCount(int last_click_count,
                       float x,
                       float y,
                       float last_x,
                       float last_y,
                       int button_id,
                       int last_button_id,
                       const base::TimeTicks& timestamp,
                       const base::TimeTicks& last_mouse_click_time) {
  const int kDoubleClickTimeMS = 500;
  const int kDoubleClickRange = 4;
  if (last_click_count == 0)
    return 1;

  base::TimeDelta time_difference = timestamp - last_mouse_click_time;
  if (time_difference.InMilliseconds() > kDoubleClickTimeMS)
    return 1;

  if (std::abs(x - last_x) > kDoubleClickRange / 2)
    return 1;

  if (std::abs(y - last_y) > kDoubleClickRange / 2)
    return 1;

  if (last_button_id != button_id)
    return 1;

#if !defined(OS_MAC) && !defined(OS_WIN)
  // On Mac and Windows, we keep increasing the click count, but on the other
  // platforms, we reset the count to 1 when it is greater than 3.
  if (last_click_count >= 3)
    return 1;
#endif
  return last_click_count + 1;
}

const char kLandscape[] = "landscape";
const char kPortrait[] = "portrait";

Status ParseOrientation(const base::DictionaryValue& params,
                        std::string* orientation) {
  bool has_value;
  if (!GetOptionalString(&params, "orientation", orientation, &has_value)) {
    return Status(kInvalidArgument, "'orientation' must be a string");
  }

  if (!has_value) {
    *orientation = kPortrait;
  } else if (*orientation != kPortrait && *orientation != kLandscape) {
    return Status(kInvalidArgument, "'orientation' must be '" +
                                        std::string(kPortrait) + "' or '" +
                                        std::string(kLandscape) + "'");
  }
  return Status(kOk);
}

Status ParseScale(const base::DictionaryValue& params, double* scale) {
  bool has_value;
  if (!GetOptionalDouble(&params, "scale", scale, &has_value)) {
    return Status(kInvalidArgument, "'scale' must be a double");
  }

  if (!has_value) {
    *scale = 1;
  } else if (*scale < 0.1 || *scale > 2) {
    return Status(kInvalidArgument, "'scale' must not be < 0.1 or > 2");
  }
  return Status(kOk);
}

Status ParseBoolean(const base::DictionaryValue& params,
                    const std::string& name,
                    bool default_value,
                    bool* b) {
  *b = default_value;
  if (!GetOptionalBool(&params, name, b)) {
    return Status(kInvalidArgument, "'" + name + "' must be a boolean");
  }
  return Status(kOk);
}

Status GetNonNegativeDouble(const base::DictionaryValue* dict,
                            const std::string& parent,
                            const std::string& child,
                            double* attribute) {
  bool has_value;
  std::string attributeStr = "'" + parent + "." + child + "'";
  if (!GetOptionalDouble(dict, child, attribute, &has_value)) {
    return Status(kInvalidArgument, attributeStr + " must be a double");
  }

  if (has_value) {
    *attribute = ConvertCentimeterToInch(*attribute);
    if (*attribute < 0) {
      return Status(kInvalidArgument,
                    attributeStr + " must not be less than 0");
    }
  }
  return Status(kOk);
}

struct Page {
  double width;
  double height;
};

Status ParsePage(const base::DictionaryValue& params, Page* page) {
  bool has_value;
  const base::DictionaryValue* page_dict;
  if (!GetOptionalDictionary(&params, "page", &page_dict, &has_value)) {
    return Status(kInvalidArgument, "'page' must be an object");
  }
  page->width = ConvertCentimeterToInch(21.59);
  page->height = ConvertCentimeterToInch(27.94);
  if (!has_value)
    return Status(kOk);

  Status status =
      GetNonNegativeDouble(page_dict, "page", "width", &page->width);
  if (status.IsError())
    return status;

  status = GetNonNegativeDouble(page_dict, "page", "height", &page->height);
  if (status.IsError())
    return status;

  return Status(kOk);
}

struct Margin {
  double top;
  double bottom;
  double left;
  double right;
};

Status ParseMargin(const base::DictionaryValue& params, Margin* margin) {
  bool has_value;
  const base::DictionaryValue* margin_dict;
  if (!GetOptionalDictionary(&params, "margin", &margin_dict, &has_value)) {
    return Status(kInvalidArgument, "'margin' must be an object");
  }

  margin->top = ConvertCentimeterToInch(1.0);
  margin->bottom = ConvertCentimeterToInch(1.0);
  margin->left = ConvertCentimeterToInch(1.0);
  margin->right = ConvertCentimeterToInch(1.0);

  if (!has_value)
    return Status(kOk);

  Status status =
      GetNonNegativeDouble(margin_dict, "margin", "top", &margin->top);
  if (status.IsError())
    return status;

  status =
      GetNonNegativeDouble(margin_dict, "margin", "bottom", &margin->bottom);
  if (status.IsError())
    return status;

  status = GetNonNegativeDouble(margin_dict, "margin", "left", &margin->left);
  if (status.IsError())
    return status;

  status = GetNonNegativeDouble(margin_dict, "margin", "right", &margin->right);
  if (status.IsError())
    return status;

  return Status(kOk);
}

Status ParsePageRanges(const base::DictionaryValue& params,
                       std::string* pageRanges) {
  bool has_value;
  const base::ListValue* page_range_list = nullptr;
  if (!GetOptionalList(&params, "pageRanges", &page_range_list, &has_value)) {
    return Status(kInvalidArgument, "'pageRanges' must be an array");
  }

  if (!has_value) {
    return Status(kOk);
  }

  std::vector<std::string> ranges;
  int page;
  std::string pages_str;
  for (const base::Value& page_range : *page_range_list) {
    if (page_range.GetAsInteger(&page)) {
      if (page < 0) {
        return Status(kInvalidArgument,
                      "a Number entry in 'pageRanges' must not be less than 0");
      }
      ranges.push_back(base::NumberToString(page));
    } else if (page_range.GetAsString(&pages_str)) {
      ranges.push_back(pages_str);
    } else {
      return Status(kInvalidArgument,
                    "an entry in 'pageRanges' must be a Number or String");
    }
  }

  *pageRanges = base::JoinString(ranges, ",");
  return Status(kOk);
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
    const std::string& prompt_behavior = session->unhandled_prompt_behavior;

    if (prompt_behavior == kAccept || prompt_behavior == kAcceptAndNotify)
      status = dialog_manager->HandleDialog(true, session->prompt_text.get());
    else if (prompt_behavior == kDismiss ||
             prompt_behavior == kDismissAndNotify)
      status = dialog_manager->HandleDialog(false, session->prompt_text.get());
    if (status.IsError())
      return status;

    // For backward compatibility, in legacy mode we always notify.
    if (!session->w3c_compliant || prompt_behavior == kAcceptAndNotify ||
        prompt_behavior == kDismissAndNotify || prompt_behavior == kIgnore)
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
    } else if (status.code() == kUnknownError && web_view->IsNonBlocking() &&
               status.message().find(kTargetClosedMessage) !=
                   std::string::npos) {
      // When pageload strategy is None, new navigation can occur during
      // execution of a command. Retry the command.
      continue;
    } else if (status.IsError()) {
      // If the command failed while a new page or frame started loading, retry
      // the command after the pending navigation has completed.
      bool is_pending = false;
      nav_status = web_view->IsPendingNavigation(&timeout, &is_pending);
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
  if (status.code() == kUnexpectedAlertOpen_Keep)
    return Status(kUnexpectedAlertOpen, status.message());
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
    // Need to support line oriented comment
    if (script.find("//") != std::string::npos)
      script = script + "\n";

    Status status =
        web_view->CallUserSyncScript(session->GetCurrentFrameId(), script,
                                     *args, session->script_timeout, value);
    if (status.code() == kTimeout)
      return Status(kScriptTimeout);
    return status;
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

  // Need to support line oriented comment
  if (script.find("//") != std::string::npos)
    script = script + "\n";

  Status status = web_view->CallUserAsyncFunction(
      session->GetCurrentFrameId(), "async function(){" + script + "}", *args,
      session->script_timeout, value);
  if (status.code() == kTimeout)
    return Status(kScriptTimeout);
  return status;
}

Status ExecuteNewWindow(Session* session,
                        WebView* web_view,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  std::string type;
  // "type" can either be None or a string.
  auto* type_param = params.FindKey("type");
  if (!(!type_param || type_param->is_none() ||
        params.GetString("type", &type)))
    return Status(kInvalidArgument, "missing or invalid 'type'");

  // By default, creates new tab.
  Chrome::WindowType window_type = (type == "window")
                                       ? Chrome::WindowType::kWindow
                                       : Chrome::WindowType::kTab;

  std::string handle;
  Status status =
      session->chrome->NewWindow(session->window, window_type, &handle);

  if (status.IsError())
    return status;

  auto results = std::make_unique<base::DictionaryValue>();
  results->SetString("handle", WebViewIdToWindowHandle(handle));
  results->SetString(
      "type", (window_type == Chrome::WindowType::kWindow) ? "window" : "tab");
  *value = std::move(results);
  return Status(kOk);
}

Status ExecuteSwitchToFrame(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const base::Value* id;
  if (!params.Get("id", &id))
    return Status(kInvalidArgument, "missing 'id'");

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
      return Status(kInvalidArgument, "missing 'ELEMENT'");
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
      if (session->w3c_compliant)
        return Status(kInvalidArgument, "'id' can not be string");
      else
        xpath += base::StringPrintf(
          "[@name=\"%s\" or @id=\"%s\"]", id_string.c_str(), id_string.c_str());
    } else if (id->GetAsInteger(&id_int)) {
      const int max_range = 65535; // 2^16 - 1
      if (id_int < 0 || id_int > max_range)
        return Status(kInvalidArgument, "'id' out of range");
      else
        xpath += base::StringPrintf("[%d]", id_int + 1);
    } else {
      return Status(kInvalidArgument, "invalid 'id'");
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
      " () => (document.documentElement || {}).outerHTML || ''";

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
  *value = std::make_unique<base::Value>(url);
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
    return Status(kInvalidArgument,
                  "at least an element or offset should be set");

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

  std::vector<MouseEvent> events;
  events.push_back(MouseEvent(kMovedMouseEventType,
                              session->pressed_mouse_button, location.x,
                              location.y, session->sticky_modifiers, 0, 0));
  Status status = web_view->DispatchMouseEvents(
      events, session->GetCurrentFrameId(), false);
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
  std::vector<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers, 0, 1));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers,
                 MouseButtonToButtons(button), 1));
  session->pressed_mouse_button = kNoneMouseButton;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId(),
                                       false);
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
  std::vector<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers, 0, 1));
  session->pressed_mouse_button = button;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId(),
                                       false);
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
  std::vector<MouseEvent> events;
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers,
                 MouseButtonToButtons(button), 1));
  session->pressed_mouse_button = kNoneMouseButton;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId(),
                                       false);
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
  std::vector<MouseEvent> events;
  events.push_back(
      MouseEvent(kPressedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers, 0, 1));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers,
                 MouseButtonToButtons(button), 1));
  events.push_back(
      MouseEvent(kPressedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers, 0, 2));
  events.push_back(
      MouseEvent(kReleasedMouseEventType, button, session->mouse_position.x,
                 session->mouse_position.y, session->sticky_modifiers,
                 MouseButtonToButtons(button), 2));
  session->pressed_mouse_button = kNoneMouseButton;
  return web_view->DispatchMouseEvents(events, session->GetCurrentFrameId(),
                                       false);
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
    return Status(kInvalidArgument, "'xoffset' must be an integer");
  int yoffset;
  if (!params.GetInteger("yoffset", &yoffset))
    return Status(kInvalidArgument, "'yoffset' must be an integer");
  return web_view->SynthesizeScrollGesture(
      location.x, location.y, xoffset, yoffset);
}

Status ProcessInputActionSequence(
    Session* session,
    const base::DictionaryValue* action_sequence,
    std::vector<std::unique_ptr<base::DictionaryValue>>* action_list) {
  std::string id;
  std::string type;
  const base::DictionaryValue* source;
  const base::DictionaryValue* parameters;
  std::string pointer_type;
  if (!action_sequence->GetString("type", &type) ||
      ((type != "key") && (type != "pointer") && (type != "wheel") &&
       (type != "none"))) {
    return Status(kInvalidArgument,
                  "'type' must be one of the strings 'key', 'pointer', 'wheel' "
                  "or 'none'");
  }

  if (!action_sequence->GetString("id", &id))
    return Status(kInvalidArgument, "'id' must be a string");

  if (type == "pointer") {
    if (action_sequence->GetDictionary("parameters", &parameters)) {
      // error check arguments
      if (!parameters->GetString("pointerType", &pointer_type) ||
          (pointer_type != "mouse" && pointer_type != "pen" &&
           pointer_type != "touch"))
        return Status(
            kInvalidArgument,
            "'pointerType' must be a string and one of mouse, pen or touch");
    } else {
      pointer_type = "mouse";
    }
  }

  bool found = false;
  for (size_t i = 0; i < session->active_input_sources.GetSize(); i++) {
    session->active_input_sources.GetDictionary(i, &source);
    DCHECK(source);

    std::string source_id;
    std::string source_type;
    source->GetString("id", &source_id);
    source->GetString("type", &source_type);
    if (source_id == id && source_type == type) {
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
      break;
    }
  }

  // if we found no matching active input source
  std::unique_ptr<base::DictionaryValue> tmp_source(new base::DictionaryValue);
  if (!found) {
    // create input source
    tmp_source->SetString("id", id);
    tmp_source->SetString("type", type);
    if (type == "pointer") {
      tmp_source->SetString("pointerType", pointer_type);
    }

    session->active_input_sources.Append(std::move(tmp_source));

    base::DictionaryValue tmp_state;
    tmp_state.SetString("id", id);
    if (type == "key") {
      // Initialize a key input state object
      // (https://w3c.github.io/webdriver/#dfn-key-input-state).
      tmp_state.SetDictionary("pressed",
                              std::make_unique<base::DictionaryValue>());
      // For convenience, we use one integer property to encode four Boolean
      // properties (alt, shift, ctrl, meta) from the spec, using values from
      // enum KeyModifierMask.
      tmp_state.SetInteger("modifiers", 0);
    } else if (type == "pointer") {
      int x = 0;
      int y = 0;

      // "pressed" is stored as a bitmask of pointer buttons.
      tmp_state.SetInteger("pressed", 0);
      tmp_state.SetString("subtype", pointer_type);

      tmp_state.SetInteger("x", x);
      tmp_state.SetInteger("y", y);
    }
    session->input_state_table.SetDictionary(
        id, std::make_unique<base::DictionaryValue>(std::move(tmp_state)));
  }

  const base::ListValue* actions;
  if (!action_sequence->GetList("actions", &actions))
    return Status(kInvalidArgument, "'actions' must be an array");

  std::unique_ptr<base::ListValue> actions_result(new base::ListValue);
  for (size_t i = 0; i < actions->GetSize(); i++) {
    std::unique_ptr<base::DictionaryValue> action(new base::DictionaryValue());

    const base::DictionaryValue* action_item;
    if (!actions->GetDictionary(i, &action_item))
      return Status(
          kInvalidArgument,
          "each argument in the action sequence must be a dictionary");

    action->SetString("id", id);
    action->SetString("type", type);

    if (type == "none") {
      // process none action
      std::string subtype;
      if (!action_item->GetString("type", &subtype) || subtype != "pause")
        return Status(kInvalidArgument,
                      "type of action must be the  string 'pause'");

      action->SetString("subtype", subtype);

      Status status = ProcessPauseAction(action_item, action.get());
      if (status.IsError())
        return status;
    } else if (type == "key") {
      // process key action
      std::string subtype;
      if (!action_item->GetString("type", &subtype) ||
          (subtype != "keyUp" && subtype != "keyDown" && subtype != "pause"))
        return Status(
            kInvalidArgument,
            "type of action must be the string 'keyUp', 'keyDown' or 'pause'");

      action->SetString("subtype", subtype);
      if (subtype == "pause") {
        Status status = ProcessPauseAction(action_item, action.get());
        if (status.IsError())
          return status;
      } else {
        std::string key;
        bool valid = action_item->GetString("value", &key);
        if (valid) {
          // check if key is a single unicode code point
          int32_t char_index = 0;
          uint32_t code_point;
          valid =
              base::ReadUnicodeCharacter(key.c_str(), key.size(), &char_index,
                                         &code_point) &&
              static_cast<std::string::size_type>(char_index + 1) == key.size();
        }
        if (!valid)
          return Status(kInvalidArgument,
                        "'value' must be a single Unicode code point");
        action->SetString("value", key);
      }
    } else if (type == "pointer" || type == "wheel") {
      std::string subtype;
      if (type == "pointer") {
        if (!action_item->GetString("type", &subtype) ||
            (subtype != "pointerUp" && subtype != "pointerDown" &&
             subtype != "pointerMove" && subtype != "pointerCancel" &&
             subtype != "pause")) {
          return Status(kInvalidArgument,
                        "type of pointer action must be the string "
                        "'pointerUp', 'pointerDown', 'pointerMove' or "
                        "'pause'");
        }
      } else {
        if (!action_item->GetString("type", &subtype) ||
            (subtype != "scroll" && subtype != "pause")) {
          return Status(
              kInvalidArgument,
              "type of action must be the string 'scroll' or 'pause'");
        }
      }

      action->SetString("subtype", subtype);
      action->SetString("pointerType", pointer_type);

      if (subtype == "pointerDown" || subtype == "pointerUp") {
        if (pointer_type == "mouse" || pointer_type == "pen") {
          int button;
          if (!action_item->GetInteger("button", &button) || button < 0 ||
              button > 4) {
            return Status(
                kInvalidArgument,
                "'button' must be a non-negative int and between 0 and 4");
          }
          std::string button_str;
          Status status = IntToStringButton(button, button_str);
          if (status.IsError())
            return status;
          action->SetString("button", button_str);
        }
      } else if (subtype == "pointerMove" || subtype == "scroll") {
        int x;
        if (!action_item->GetInteger("x", &x))
          return Status(kInvalidArgument, "'x' must be an int");
        int y;
        if (!action_item->GetInteger("y", &y))
          return Status(kInvalidArgument, "'y' must be an int");
        action->SetInteger("x", x);
        action->SetInteger("y", y);

        std::string origin;
        if (action_item->HasKey("origin")) {
          if (!action_item->GetString("origin", &origin)) {
            const base::DictionaryValue* origin_dict;
            if (!action_item->GetDictionary("origin", &origin_dict))
              return Status(kInvalidArgument,
                            "'origin' must be either a string or a dictionary");
            std::string element_id;
            if (!origin_dict->GetString(GetElementKey(), &element_id))
              return Status(kInvalidArgument, "'element' is missing");
            std::unique_ptr<base::DictionaryValue> origin_result =
                std::make_unique<base::DictionaryValue>();
            origin_result->SetString(GetElementKey(), element_id);
            action->SetDictionary("origin", std::move(origin_result));
          } else {
            if (origin != "viewport" && origin != "pointer")
              return Status(kInvalidArgument,
                            "if 'origin' is a string, it must be either "
                            "'viewport' or 'pointer'");
            action->SetString("origin", origin);
          }
        } else {
          action->SetString("origin", "viewport");
        }

        Status status = ProcessPauseAction(action_item, action.get());
        if (status.IsError())
          return status;

        if (subtype == "scroll") {
          int delta_x;
          if (!action_item->GetInteger("deltaX", &delta_x))
            return Status(kInvalidArgument, "'delta x' must be an int");
          int delta_y;
          if (!action_item->GetInteger("deltaY", &delta_y))
            return Status(kInvalidArgument, "'delta y' must be an int");
          action->SetInteger("deltaX", delta_x);
          action->SetInteger("deltaY", delta_y);
        }
      } else if (subtype == "pause") {
        Status status = ProcessPauseAction(action_item, action.get());
        if (status.IsError())
          return status;
      }

      // Process Pointer Event's properties.
      double width = 1;
      if (action_item->HasKey("width") &&
          (!action_item->GetDouble("width", &width) || width < 0)) {
        return Status(kInvalidArgument,
                      "'width' must be a non-negative number");
      }
      action->SetDouble("width", width);
      double height = 1;
      if (action_item->HasKey("height") &&
          (!action_item->GetDouble("height", &height) || height < 0)) {
        return Status(kInvalidArgument,
                      "'height' must be a non-negative number");
      }
      action->SetDouble("height", height);
      double pressure = 0.5;
      if (action_item->HasKey("pressure") &&
          (!action_item->GetDouble("pressure", &pressure) || pressure < 0 ||
           pressure > 1)) {
        return Status(
            kInvalidArgument,
            "'pressure' must be a non-negative number in the range of [0,1]");
      }
      action->SetDouble("pressure", pressure);
      double tangentialPressure = 0;
      if (action_item->HasKey("tangentialPressure") &&
          (!action_item->GetDouble("tangentialPressure", &tangentialPressure) ||
           tangentialPressure < -1 || tangentialPressure > 1)) {
        return Status(
            kInvalidArgument,
            "'tangentialPressure' must be a number in the range of [-1,1]");
      }
      action->SetDouble("tangentialPressure", tangentialPressure);
      int tiltX = 0;
      if (action_item->HasKey("tiltX") &&
          (!action_item->GetInteger("tiltX", &tiltX) || tiltX < -90 ||
           tiltX > 90)) {
        return Status(kInvalidArgument,
                      "'tiltX' must be an integer in the range of [-90,90]");
      }
      action->SetInteger("tiltX", tiltX);
      int tiltY = 0;
      if (action_item->HasKey("tiltY") &&
          (!action_item->GetInteger("tiltY", &tiltY) || tiltY < -90 ||
           tiltY > 90)) {
        return Status(kInvalidArgument,
                      "'tiltY' must be an integer in the range of [-90,90]");
      }
      action->SetInteger("tiltY", tiltY);
      int twist = 0;
      if (action_item->HasKey("twist") &&
          (!action_item->GetInteger("twist", &twist) || twist < 0 ||
           twist > 359)) {
        return Status(kInvalidArgument,
                      "'twist' must be an integer in the range of [0,359]");
      }
      action->SetInteger("twist", twist);
    }
    action_list->push_back(std::move(action));
  }
  return Status(kOk);
}

Status ExecutePerformActions(Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  // extract action sequence
  const base::ListValue* actions_input;

  if (!params.GetList("actions", &actions_input))
    return Status(kInvalidArgument, "'actions' must be an array");

  // the processed actions
  std::vector<std::vector<std::unique_ptr<base::DictionaryValue>>> actions_list;
  for (size_t i = 0; i < actions_input->GetSize(); i++) {
    // proccess input action sequence
    const base::DictionaryValue* action_sequence;
    if (!actions_input->GetDictionary(i, &action_sequence))
      return Status(kInvalidArgument, "each argument must be a dictionary");

    std::vector<std::unique_ptr<base::DictionaryValue>> action_list;
    Status status =
        ProcessInputActionSequence(session, action_sequence, &action_list);
    actions_list.push_back(std::move(action_list));

    if (status.IsError())
      return Status(kInvalidArgument, status);
  }

  std::set<std::string> pointer_id_set;
  std::vector<base::DictionaryValue*> action_input_states;
  std::map<std::string, gfx::Point> action_locations;
  std::map<std::string, bool> has_touch_start;
  std::map<std::string, int> buttons;
  std::map<std::string, int> last_pressed_buttons;
  std::map<std::string, std::string> button_type;
  int viewport_width = 0, viewport_height = 0;
  int init_x = 0, init_y = 0;

  size_t longest_action_list_size = 0;
  for (size_t i = 0; i < actions_list.size(); i++) {
    longest_action_list_size =
        std::max(longest_action_list_size, actions_list[i].size());
  }

  for (size_t i = 0; i < longest_action_list_size; i++) {
    // Find the last pointer action, and it has to be sent synchronously by
    // default.
    size_t last_action_index = 0;
    size_t last_touch_index = 0;
    for (size_t j = 0; j < actions_list.size(); j++) {
      if (actions_list[j].size() > i) {
        const base::DictionaryValue* action = actions_list[j][i].get();
        std::string type;
        std::string action_type;
        action->GetString("type", &type);
        action->GetString("subtype", &action_type);
        if (type != "none" && action_type != "pause")
          last_action_index = j;

        if (type == "pointer") {
          std::string pointer_type;
          action->GetString("pointerType", &pointer_type);
          if (pointer_type == "touch")
            last_touch_index = j;
        }
      }
    }

    // Implements "compute the tick duration" algorithm from W3C spec
    // (https://w3c.github.io/webdriver/#dfn-computing-the-tick-duration).
    // This is the duration for actions in one tick.
    int tick_duration = 0;
    std::vector<TouchEvent> dispatch_touch_events;
    for (size_t j = 0; j < actions_list.size(); j++) {
      if (actions_list[j].size() > i) {
        const base::DictionaryValue* action = actions_list[j][i].get();
        std::string id;
        std::string type;
        std::string action_type;
        action->GetString("id", &id);
        base::DictionaryValue* input_state;
        if (!session->input_state_table.GetDictionary(id, &input_state))
          return Status(kUnknownError, "missing input state");

        action->GetString("type", &type);
        if (i == 0) {
          if (pointer_id_set.find(id) != pointer_id_set.end())
            return Status(kInvalidArgument, "'id' already exists");
          pointer_id_set.insert(id);
          action_input_states.push_back(input_state);

          if (type == "pointer" || type == "wheel") {
            Status status = WindowViewportSize(
                session, web_view, &viewport_width, &viewport_height);
            if (status.IsError())
              return status;

            input_state->GetInteger("x", &init_x);
            input_state->GetInteger("y", &init_y);
            action_locations.insert(
                std::make_pair(id, gfx::Point(init_x, init_y)));

            std::string pointer_type;
            action->GetString("pointerType", &pointer_type);
            if (pointer_type == "mouse" || pointer_type == "pen") {
              buttons[id] = input_state->FindKey("pressed")->GetInt();
              last_pressed_buttons[id] = buttons[id];
            } else if (pointer_type == "touch") {
              has_touch_start[id] = false;
            }
          }
        }

        action->GetString("subtype", &action_type);
        int duration = 0;
        if (action_type == "pause") {
          GetOptionalInt(action, "duration", &duration);
          tick_duration = std::max(tick_duration, duration);
        }

        if (type != "none") {
          bool async_dispatch_event = true;
          if (j == last_action_index) {
            async_dispatch_event = false;
            GetOptionalBool(action, "asyncDispatch", &async_dispatch_event);
          }

          if (type == "key") {
            if (action_type != "pause") {
              std::vector<KeyEvent> dispatch_key_events;
              KeyEventBuilder builder;
              Status status = ConvertKeyActionToKeyEvent(
                  action, input_state, action_type == "keyDown",
                  &dispatch_key_events);
              if (status.IsError())
                return status;

              if (dispatch_key_events.size() > 0) {
                const KeyEvent& event = dispatch_key_events.front();
                if (action_type == "keyDown") {
                  session->input_cancel_list.emplace_back(
                      action_input_states[j], nullptr, nullptr, &event);
                  session->sticky_modifiers |= KeyToKeyModifiers(event.key);
                } else if (action_type == "keyUp") {
                  session->sticky_modifiers &= ~KeyToKeyModifiers(event.key);
                }

                Status status = web_view->DispatchKeyEvents(
                    dispatch_key_events, async_dispatch_event);
                if (status.IsError())
                  return status;
              }
            }
          } else if (type == "pointer" || type == "wheel") {
            double x = 0, y = 0;
            OriginType origin = kViewPort;
            std::string element_id;
            if (action_type == "pointerMove" || action_type == "scroll") {
              action->GetDouble("x", &x);
              action->GetDouble("y", &y);
              const base::DictionaryValue* origin_dict;
              if (action->HasKey("origin")) {
                if (action->GetDictionary("origin", &origin_dict)) {
                  origin = kElement;
                  origin_dict->GetString(GetElementKey(), &element_id);
                  if (!element_id.empty()) {
                    int center_x = 0, center_y = 0;
                    Status status = ElementInViewCenter(
                        session, web_view, element_id, &center_x, &center_y);
                    if (status.IsError())
                      return status;
                    x += center_x;
                    y += center_y;
                  }
                } else {
                  std::string origin_str;
                  action->GetString("origin", &origin_str);
                  if (origin_str == "pointer") {
                    origin = kPointer;
                    x += action_locations[id].x();
                    y += action_locations[id].y();
                  }
                }
              }
              if (x < 0 || x > viewport_width || y < 0 || y > viewport_height)
                return Status(kMoveTargetOutOfBounds);

              action_locations[id] = gfx::Point(x, y);

              duration = 0;
              GetOptionalInt(action, "duration", &duration);
              tick_duration = std::max(tick_duration, duration);

              if (action_type == "scroll") {
                int delta_x = 0, delta_y = 0;
                action->GetInteger("deltaX", &delta_x);
                action->GetInteger("deltaY", &delta_y);
                std::vector<MouseEvent> dispatch_wheel_events;
                MouseEvent event(StringToMouseEventType(action_type),
                                 StringToMouseButton(button_type[id]),
                                 action_locations[id].x(),
                                 action_locations[id].y(), 0, buttons[id], 0);
                event.modifiers = session->sticky_modifiers;
                event.delta_x = delta_x;
                event.delta_y = delta_y;
                buttons[id] |= StringToModifierMouseButton(button_type[id]);
                last_pressed_buttons[id] =
                    StringToModifierMouseButton(button_type[id]);
                session->mouse_position = WebPoint(event.x, event.y);
                dispatch_wheel_events.push_back(event);
                Status status = web_view->DispatchMouseEvents(
                    dispatch_wheel_events, session->GetCurrentFrameId(),
                    async_dispatch_event);
                if (status.IsError())
                  return status;
              }
            }

            double width = 1, height = 1;
            action->GetDouble("width", &width);
            action->GetDouble("height", &height);
            double pressure = 0.5, tangential_pressure = 0;
            action->GetDouble("pressure", &pressure);
            action->GetDouble("tangentialPressure", &tangential_pressure);
            int tilt_x = 0, tilt_y = 0, twist = 0;
            action->GetInteger("tiltX", &tilt_x);
            action->GetInteger("tiltY", &tilt_y);
            action->GetInteger("twist", &twist);

            std::string pointer_type;
            action->GetString("pointerType", &pointer_type);
            if (pointer_type == "mouse" || pointer_type == "pen") {
              if (action_type != "pause") {
                std::vector<MouseEvent> dispatch_mouse_events;
                int click_count = 0;
                if (action_type == "pointerDown" ||
                    action_type == "pointerUp") {
                  std::string button;
                  action->GetString("button", &button);
                  button_type[id] = button;
                  click_count = 1;
                } else if (buttons[id] == 0) {
                  button_type[id].clear();
                }

                MouseEvent event(StringToMouseEventType(action_type),
                                 StringToMouseButton(button_type[id]),
                                 action_locations[id].x(),
                                 action_locations[id].y(), 0, buttons[id],
                                 click_count);
                event.pointer_type = StringToPointerType(pointer_type);
                event.modifiers = session->sticky_modifiers;
                event.tangentialPressure = tangential_pressure;
                event.tiltX = tilt_x;
                event.tiltY = tilt_y;
                event.twist = twist;

                if (event.type == kPressedMouseEventType) {
                  base::TimeTicks timestamp = base::TimeTicks::Now();
                  event.click_count = GetMouseClickCount(
                      session->click_count, event.x, event.y,
                      session->mouse_position.x, session->mouse_position.y,
                      StringToModifierMouseButton(button_type[id]),
                      last_pressed_buttons[id], timestamp,
                      session->mouse_click_timestamp);
                  buttons[id] |= StringToModifierMouseButton(button_type[id]);
                  last_pressed_buttons[id] =
                      StringToModifierMouseButton(button_type[id]);
                  session->mouse_position = WebPoint(event.x, event.y);
                  session->click_count = event.click_count;
                  session->mouse_click_timestamp = timestamp;
                  session->input_cancel_list.emplace_back(
                      action_input_states[j], &event, nullptr, nullptr);
                  action_input_states[j]->SetInteger(
                      "pressed",
                      action_input_states[j]->FindKey("pressed")->GetInt() |
                          (1 << event.button));
                } else if (event.type == kReleasedMouseEventType) {
                  pressure = 0;
                  event.click_count = session->click_count;
                  buttons[id] &= ~StringToModifierMouseButton(button_type[id]);
                  action_input_states[j]->SetInteger(
                      "pressed",
                      action_input_states[j]->FindKey("pressed")->GetInt() &
                          ~(1 << event.button));
                } else if (event.type == kMovedMouseEventType) {
                  if (action_input_states[j]->FindKey("pressed")->GetInt() ==
                      0) {
                    pressure = 0;
                  }
                }
                event.force = pressure;
                dispatch_mouse_events.push_back(event);
                Status status = web_view->DispatchMouseEvents(
                    dispatch_mouse_events, session->GetCurrentFrameId(),
                    async_dispatch_event);
                if (status.IsError())
                  return status;
              }
            } else if (pointer_type == "touch") {
              if (action_type == "pointerDown")
                has_touch_start[id] = true;
              TouchEvent event(StringToTouchEventType(action_type),
                               action_locations[id].x(),
                               action_locations[id].y());
              event.radiusX = width / 2.f;
              event.radiusY = height / 2.f;
              event.force = pressure;
              event.tangentialPressure = tangential_pressure;
              event.tiltX = tilt_x;
              event.tiltY = tilt_y;
              event.twist = twist;
              if (event.type == kTouchStart) {
                session->input_cancel_list.emplace_back(
                    action_input_states[j], nullptr, &event, nullptr);
                action_input_states[j]->SetInteger("pressed", 1);
              } else if (event.type == kTouchEnd) {
                action_input_states[j]->SetInteger("pressed", 0);
              }
              if (has_touch_start[id]) {
                if (event.type == kPause)
                  event.type = kTouchMove;
                event.id = j;
                dispatch_touch_events.push_back(event);
              }
              if (j == last_touch_index) {
                Status status = web_view->DispatchTouchEventWithMultiPoints(
                    dispatch_touch_events, async_dispatch_event);
                if (status.IsError())
                  return status;
              }
              if (action_type == "pointerUp")
                has_touch_start[id] = false;
            }
            action_input_states[j]->SetInteger("x", action_locations[id].x());
            action_input_states[j]->SetInteger("y", action_locations[id].y());
          }
        }
      }
    }

    if (tick_duration > 0) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(tick_duration));
    }
  }

  return Status(kOk);
}

Status ExecuteReleaseActions(Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  for (auto it = session->input_cancel_list.rbegin();
       it != session->input_cancel_list.rend(); ++it) {
    if (it->key_event) {
      base::DictionaryValue* pressed;
      it->input_state->GetDictionary("pressed", &pressed);
      if (!pressed->HasKey(it->key_event->key))
        continue;
      web_view->DispatchKeyEvents({*it->key_event}, false);
      pressed->Remove(it->key_event->key, nullptr);
    } else if (it->mouse_event) {
      int pressed = it->input_state->FindKey("pressed")->GetInt();
      int button_mask = 1 << it->mouse_event->button;
      if ((pressed & button_mask) == 0)
        continue;
      web_view->DispatchMouseEvents({*it->mouse_event},
                                    session->GetCurrentFrameId(), false);
      it->input_state->SetInteger("pressed", pressed & ~button_mask);
    } else if (it->touch_event) {
      int pressed = it->input_state->FindKey("pressed")->GetInt();
      if (pressed == 0)
        continue;
      web_view->DispatchTouchEvents({*it->touch_event}, false);
      it->input_state->SetInteger("pressed", 0);
    }
  }

  session->input_cancel_list.clear();
  session->input_state_table.Clear();
  session->active_input_sources.Clear();
  session->mouse_position = WebPoint(0, 0);
  session->click_count = 0;
  session->mouse_click_timestamp = base::TimeTicks::Now();
  session->sticky_modifiers = 0;

  return Status(kOk);
}

Status ExecuteSendCommand(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  std::string cmd;
  if (!params.GetString("cmd", &cmd)) {
    return Status(kInvalidArgument, "command not passed");
  }
  const base::DictionaryValue* cmdParams;
  if (!params.GetDictionary("params", &cmdParams)) {
    return Status(kInvalidArgument, "params not passed");
  }
  return web_view->SendCommand(cmd, *cmdParams);
}

Status ExecuteSendCommandFromWebSocket(Session* session,
                                       WebView* web_view,
                                       const base::DictionaryValue& params,
                                       std::unique_ptr<base::Value>* value,
                                       Timeout* timeout) {
  std::string cmd;
  if (!params.GetString("method", &cmd)) {
    return Status(kInvalidArgument, "command not passed");
  }
  const base::DictionaryValue* cmdParams;
  if (!params.GetDictionary("params", &cmdParams)) {
    return Status(kInvalidArgument, "params not passed");
  }
  int client_cmd_id;
  if (!params.GetInteger("id", &client_cmd_id) ||
      !CommandId::IsClientCommandId(client_cmd_id)) {
    return Status(kInvalidArgument, "command id must be negative");
  }

  return web_view->SendCommandFromWebSocket(cmd, *cmdParams, client_cmd_id);
}

Status ExecuteSendCommandAndGetResult(Session* session,
                                      WebView* web_view,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  std::string cmd;
  if (!params.GetString("cmd", &cmd)) {
    return Status(kInvalidArgument, "command not passed");
  }
  const base::DictionaryValue* cmdParams;
  if (!params.GetDictionary("params", &cmdParams)) {
    return Status(kInvalidArgument, "params not passed");
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
    return Status(kInvalidArgument, "'value' must be a list");
  return SendKeysOnWindow(
      web_view, key_list, false, &session->sticky_modifiers);
}

Status ExecuteGetStorageItem(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  std::string key;
  if (!params.GetString("key", &key))
    return Status(kInvalidArgument, "'key' must be a string");
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
  return web_view->EvaluateScript(session->GetCurrentFrameId(),
                                  base::StringPrintf(script, storage), false,
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
    return Status(kInvalidArgument, "'key' must be a string");
  std::string storage_value;
  if (!params.GetString("value", &storage_value))
    return Status(kInvalidArgument, "'value' must be a string");
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
    return Status(kInvalidArgument, "'key' must be a string");
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
  return web_view->EvaluateScript(session->GetCurrentFrameId(),
                                  base::StringPrintf("%s.clear()", storage),
                                  false, value);
}

Status ExecuteGetStorageSize(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  return web_view->EvaluateScript(session->GetCurrentFrameId(),
                                  base::StringPrintf("%s.length", storage),
                                  false, value);
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
  status = web_view->CaptureScreenshot(&screenshot, base::DictionaryValue());
  if (status.IsError()) {
    if (status.code() == kUnexpectedAlertOpen) {
      LOG(WARNING) << status.message() << ", cancelling screenshot";
      // we can't take screenshot in this state
      // but we must return kUnexpectedAlertOpen_Keep instead
      // see https://crbug.com/chromedriver/2117
      return Status(kUnexpectedAlertOpen_Keep);
    }
    LOG(WARNING) << "screenshot failed, retrying " << status.message();
    status = web_view->CaptureScreenshot(&screenshot, base::DictionaryValue());
  }
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(screenshot);
  return Status(kOk);
}

Status ExecuteFullPageScreenshot(Session* session,
                                 WebView* web_view,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value,
                                 Timeout* timeout) {
  Status status = session->chrome->ActivateWebView(web_view->GetId());
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> layoutMetrics;
  status = web_view->SendCommandAndGetResult(
      "Page.getLayoutMetrics", base::DictionaryValue(), &layoutMetrics);
  if (status.IsError())
    return status;

  const auto width = layoutMetrics->FindDoublePath("contentSize.width");
  if (!width.has_value())
    return Status(kUnknownError, "invalid width type");
  int w = ceil(width.value());
  if (w == 0)
    return Status(kUnknownError, "invalid width 0");

  const auto height = layoutMetrics->FindDoublePath("contentSize.height");
  if (!height.has_value())
    return Status(kUnknownError, "invalid height type");
  int h = ceil(height.value());
  if (h == 0)
    return Status(kUnknownError, "invalid height 0");

  auto* meom = web_view->GetMobileEmulationOverrideManager();
  bool hasOverrideMetrics = meom->HasOverrideMetrics();

  base::DictionaryValue deviceMetrics;
  deviceMetrics.SetInteger("width", w);
  deviceMetrics.SetInteger("height", h);
  if (hasOverrideMetrics) {
    const auto* dm = meom->GetDeviceMetrics();
    deviceMetrics.SetInteger("deviceScaleFactor", dm->device_scale_factor);
    deviceMetrics.SetBoolean("mobile", dm->mobile);
  } else {
    deviceMetrics.SetInteger("deviceScaleFactor", 1);
    deviceMetrics.SetBoolean("mobile", false);
  }
  std::unique_ptr<base::Value> ignore;
  status = web_view->SendCommandAndGetResult(
      "Emulation.setDeviceMetricsOverride", deviceMetrics, &ignore);
  if (status.IsError())
    return status;

  std::string screenshot;
  // No need to supply clip as it would be default to the device metrics
  // parameters
  status = web_view->CaptureScreenshot(&screenshot, base::DictionaryValue());
  if (status.IsError()) {
    if (status.code() == kUnexpectedAlertOpen) {
      LOG(WARNING) << status.message() << ", cancelling screenshot";
      // we can't take screenshot in this state
      // but we must return kUnexpectedAlertOpen_Keep instead
      // see https://crbug.com/chromedriver/2117
      return Status(kUnexpectedAlertOpen_Keep);
    }
    LOG(WARNING) << "screenshot failed, retrying " << status.message();
    status = web_view->CaptureScreenshot(&screenshot, base::DictionaryValue());
  }
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(screenshot);

  // Check if there is already deviceMetricsOverride in use,
  // if so, restore to that instead
  if (hasOverrideMetrics) {
    status = meom->RestoreOverrideMetrics();
  } else {
    // The scroll bar disappear after setting device metrics to fullpage
    // width and height, this is to clear device metrics and restore
    // scroll bars
    status = web_view->SendCommandAndGetResult(
        "Emulation.clearDeviceMetricsOverride", base::DictionaryValue(),
        &ignore);
  }
  return status;
}

Status ExecutePrint(Session* session,
                    WebView* web_view,
                    const base::DictionaryValue& params,
                    std::unique_ptr<base::Value>* value,
                    Timeout* timeout) {
  std::string orientation;
  Status status = ParseOrientation(params, &orientation);
  if (status.IsError())
    return status;

  double scale;
  status = ParseScale(params, &scale);
  if (status.IsError())
    return status;

  bool background;
  status = ParseBoolean(params, "background", false, &background);
  if (status.IsError())
    return status;

  Page page;
  status = ParsePage(params, &page);
  if (status.IsError())
    return status;

  Margin margin;
  status = ParseMargin(params, &margin);
  if (status.IsError())
    return status;

  bool shrinkToFit;
  status = ParseBoolean(params, "shrinkToFit", true, &shrinkToFit);
  if (status.IsError())
    return status;

  std::string pageRanges;
  status = ParsePageRanges(params, &pageRanges);
  if (status.IsError())
    return status;

  base::DictionaryValue printParams;
  printParams.SetBoolean(kLandscape, orientation == kLandscape);
  printParams.SetDouble("scale", scale);
  printParams.SetBoolean("printBackground", background);
  printParams.SetDouble("paperWidth", page.width);
  printParams.SetDouble("paperHeight", page.height);
  printParams.SetDouble("marginTop", margin.top);
  printParams.SetDouble("marginBottom", margin.bottom);
  printParams.SetDouble("marginLeft", margin.left);
  printParams.SetDouble("marginRight", margin.right);
  printParams.SetBoolean("preferCSSPageSize", !shrinkToFit);
  printParams.SetString("pageRanges", pageRanges);
  printParams.SetString("transferMode", "ReturnAsBase64");

  std::string pdf;
  status = web_view->PrintToPDF(printParams, &pdf);
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(pdf);
  return Status(kOk);
}

Status ExecuteGetCookies(Session* session,
                         WebView* web_view,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(session, web_view, &cookies);
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
    return Status(kInvalidArgument, "missing 'cookie name'");

  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(session, web_view, &cookies);
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
    return Status(kInvalidArgument, "missing 'cookie'");
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
  if (!base::StartsWith(url, "http://", base::CompareCase::INSENSITIVE_ASCII) &&
      !base::StartsWith(url, "https://",
                        base::CompareCase::INSENSITIVE_ASCII) &&
      !base::StartsWith(url, "ftp://", base::CompareCase::INSENSITIVE_ASCII))
    return Status(kInvalidCookieDomain);
  std::string domain;
  if (!GetOptionalString(cookie, "domain", &domain))
    return Status(kInvalidArgument, "invalid 'domain'");
  if (session->w3c_compliant && !domain.empty() &&
      !url::HostIsIPAddress(domain)) {
    if (domain[0] == '.')
      domain = domain.substr(1);

    if (domain.size() < 2)
      return Status(kInvalidCookieDomain, "invalid 'domain'");

    if (!GURL(url).DomainIs(domain))
      return Status(kInvalidCookieDomain, "Cookie 'domain' mismatch");

    domain.insert(0, 1, '.');
  }
  std::string path("/");
  if (!GetOptionalString(cookie, "path", &path))
    return Status(kInvalidArgument, "invalid 'path'");
  std::string samesite("");
  if (!GetOptionalString(cookie, "sameSite", &samesite))
    return Status(kInvalidArgument, "invalid 'sameSite'");
  if (!samesite.empty() && samesite != "Strict" && samesite != "Lax" &&
      samesite != "None")
    return Status(kInvalidArgument, "invalid 'sameSite'");
  bool secure = false;
  if (!GetOptionalBool(cookie, "secure", &secure))
    return Status(kInvalidArgument, "invalid 'secure'");
  bool httpOnly = false;
  if (!GetOptionalBool(cookie, "httpOnly", &httpOnly))
    return Status(kInvalidArgument, "invalid 'httpOnly'");
  double expiry;
  bool has_value;
  if (session->w3c_compliant) {
    // W3C spec says expiry is a safe integer.
    int64_t expiry_int64;
    if (!GetOptionalSafeInt(cookie, "expiry", &expiry_int64, &has_value) ||
        (has_value && expiry_int64 < 0))
      return Status(kInvalidArgument, "invalid 'expiry'");
    // Use negative value to indicate expiry not specified.
    expiry = has_value ? static_cast<double>(expiry_int64) : -1.0;
  } else {
    // JSON wire protocol didn't specify the type of expiry, but ChromeDriver
    // has always accepted double, so we keep that in legacy mode.
    if (!GetOptionalDouble(cookie, "expiry", &expiry, &has_value) ||
        (has_value && expiry < 0))
      return Status(kInvalidArgument, "invalid 'expiry'");
    if (!has_value)
      expiry = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() +
               kDefaultCookieExpiryTime;
  }
  return web_view->AddCookie(name, url, cookie_value, domain, path, samesite,
                             secure, httpOnly, expiry);
}

Status ExecuteDeleteCookie(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kInvalidArgument, "missing 'name'");
  base::DictionaryValue params_url;
  std::unique_ptr<base::Value> value_url;
  std::string url;
  Status status = GetUrl(web_view, session->GetCurrentFrameId(), &url);
  if (status.IsError())
    return status;

  std::list<Cookie> cookies;
  status = GetVisibleCookies(session, web_view, &cookies);
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
  Status status = GetVisibleCookies(session, web_view, &cookies);
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
    return Status(kInvalidArgument, "missing or invalid 'location'");
  if (location->HasKey("accuracy") &&
      !location->GetDouble("accuracy", &geoposition.accuracy)) {
    return Status(kInvalidArgument, "invalid 'accuracy'");
  } else {
    // |accuracy| is not part of the WebDriver spec yet, so if it is not given
    // default to 100 meters accuracy.
    geoposition.accuracy = 100;
  }

  Status status = web_view->OverrideGeolocation(geoposition);
  if (status.IsOk()) {
    session->overridden_geoposition =
        std::make_unique<Geoposition>(geoposition);
  }
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
      return Status(kInvalidArgument,
                    "invalid 'network_conditions' is missing 'latency'");

    // Either |throughput| or the pair |download_throughput| and
    // |upload_throughput| is required.
    if (conditions->HasKey("throughput")) {
      if (!conditions->GetDouble("throughput",
                                 &network_conditions->download_throughput))
        return Status(kInvalidArgument, "invalid 'throughput'");
      conditions->GetDouble("throughput",
                            &network_conditions->upload_throughput);
    } else if (conditions->HasKey("download_throughput") &&
               conditions->HasKey("upload_throughput")) {
      if (!conditions->GetDouble("download_throughput",
                                 &network_conditions->download_throughput) ||
          !conditions->GetDouble("upload_throughput",
                                 &network_conditions->upload_throughput))
        return Status(kInvalidArgument,
                      "invalid 'download_throughput' or 'upload_throughput'");
    } else {
      return Status(kInvalidArgument,
                    "invalid 'network_conditions' is missing 'throughput' or "
                    "'download_throughput'/'upload_throughput' pair");
    }

    // |offline| is optional.
    if (conditions->HasKey("offline")) {
      if (!conditions->GetBoolean("offline", &network_conditions->offline))
        return Status(kInvalidArgument, "invalid 'offline'");
    } else {
      network_conditions->offline = false;
    }
  } else {
    return Status(kInvalidArgument,
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

Status ExecuteGetWindowRect(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  Chrome::WindowRect windowRect;
  Status status = session->chrome->GetWindowRect(session->window, &windowRect);
  if (status.IsError())
    return status;

  base::DictionaryValue rect;
  rect.SetInteger("x", windowRect.x);
  rect.SetInteger("y", windowRect.y);
  rect.SetInteger("width", windowRect.width);
  rect.SetInteger("height", windowRect.height);
  value->reset(rect.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetWindowRect(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const double max_range = 2147483647; // 2^31 - 1
  const double min_range = -2147483648; // -2^31
  const base::Value* temp;
  double width = 0;
  double height = 0;
  double x = 0;
  double y = 0;

  bool has_x = params.Get("x", &temp) && !temp->is_none();
  if (has_x) {
    if (!temp->GetAsDouble(&x))
      return Status(kInvalidArgument, "'x' must be a number");
    if (x > max_range || x < min_range)
      return Status(kInvalidArgument, "'x' out of range");
  }
  bool has_y = params.Get("y", &temp) && !temp->is_none();
  if (has_y) {
    if (!temp->GetAsDouble(&y))
      return Status(kInvalidArgument, "'y' must be a number");
    if (y > max_range || y < min_range )
      return Status(kInvalidArgument, "'y' out of range");
  }
  bool has_width = params.Get("width", &temp) && !temp->is_none();
  if (has_width) {
    if (!temp->GetAsDouble(&width))
      return Status(kInvalidArgument, "'width' must be a number");
    if (width > max_range || width < 0 )
      return Status(kInvalidArgument, "'width' out of range");
  }
  bool has_height = params.Get("height", &temp) && !temp->is_none();
  if (has_height) {
    if (!temp->GetAsDouble(&height))
      return Status(kInvalidArgument, "'height' must be a number");
    if (height > max_range || height < 0 )
      return Status(kInvalidArgument, "'height' out of range");
  }

  // to pass to the set window rect command
  base::DictionaryValue rect_params;
  // only set position if both x and y are given
  if (has_x && has_y) {
    rect_params.SetInteger("x", static_cast<int>(x));
    rect_params.SetInteger("y", static_cast<int>(y));
  }  // only set size if both height and width are given
  if (has_width && has_height) {
    rect_params.SetInteger("width", static_cast<int>(width));
    rect_params.SetInteger("height", static_cast<int>(height));
  }
  Status status = session->chrome->SetWindowRect(session->window, rect_params);
  if (status.IsError())
    return status;

  // return the current window rect
  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteMaximizeWindow(Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  Status status = session->chrome->MaximizeWindow(session->window);
  if (status.IsError())
    return status;

  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteMinimizeWindow(Session* session,
                             WebView* web_view,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  Status status = session->chrome->MinimizeWindow(session->window);
  if (status.IsError())
    return status;

  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteFullScreenWindow(Session* session,
                               WebView* web_view,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  Status status = session->chrome->FullScreenWindow(session->window);
  if (status.IsError())
    return status;

  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteSetSinkToUse(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return web_view->SendCommand("Cast.setSinkToUse", params);
}

Status ExecuteStartTabMirroring(Session* session,
                                WebView* web_view,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  return web_view->SendCommand("Cast.startTabMirroring", params);
}

Status ExecuteStopCasting(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  return web_view->SendCommand("Cast.stopCasting", params);
}

Status ExecuteGetSinks(Session* session,
                       WebView* web_view,
                       const base::DictionaryValue& params,
                       std::unique_ptr<base::Value>* value,
                       Timeout* timeout) {
  *value = web_view->GetCastSinks();
  return Status(kOk);
}

Status ExecuteGetIssueMessage(Session* session,
                              WebView* web_view,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  *value = web_view->GetCastIssueMessage();
  return Status(kOk);
}

Status ExecuteSetPermission(Session* session,
                            WebView* web_view,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const base::DictionaryValue* descriptor;
  if (!params.GetDictionary("descriptor", &descriptor))
    return Status(kInvalidArgument, "no descriptor dictionary");

  std::string name;
  if (!descriptor->GetString("name", &name))
    return Status(kInvalidArgument, "no name in descriptor");

  std::string permission_state;
  if (!params.GetString("state", &permission_state))
    return Status(kInvalidArgument, "no permission state");

  bool one_realm = false;
  if (!GetOptionalBool(&params, "oneRealm", &one_realm, nullptr))
    return Status(kInvalidArgument, "oneRealm defined but not a boolean");

  Chrome::PermissionState valid_state;
  if (permission_state == "granted")
    valid_state = Chrome::PermissionState::kGranted;
  else if (permission_state == "denied")
    valid_state = Chrome::PermissionState::kDenied;
  else if (permission_state == "prompt")
    valid_state = Chrome::PermissionState::kPrompt;
  else
    return Status(kInvalidArgument, "unrecognized permission state");

  auto val = base::Value::ToUniquePtrValue(descriptor->Clone());
  auto dict = base::DictionaryValue::From(std::move(val));

  return session->chrome->SetPermission(std::move(dict), valid_state, one_realm,
                                        web_view);
}
