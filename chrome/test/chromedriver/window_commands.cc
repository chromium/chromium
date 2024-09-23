// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/window_commands.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/element_util.h"
#include "chrome/test/chromedriver/key_converter.h"
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

Status GetMouseButton(const base::Value::Dict& params, MouseButton* button) {
  // Default to left mouse button.
  int button_num = params.FindInt("button").value_or(0);
  if (button_num < 0 || button_num > 2) {
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
  base::Value::List args;
  Status status = web_view->CallFunction(
      frame, "function() { return document.URL; }", args, &value);
  if (status.IsError())
    return status;
  if (!value->is_string())
    return Status(kUnknownError, "javascript failed to return the url");
  *url = value->GetString();
  return Status(kOk);
}

MouseEventType StringToMouseEventType(std::string action_type) {
  if (action_type == "pointerDown")
    return kPressedMouseEventType;
  if (action_type == "pointerUp")
    return kReleasedMouseEventType;
  if (action_type == "pointerMove")
    return kMovedMouseEventType;
  if (action_type == "scroll")
    return kWheelMouseEventType;
  if (action_type == "pause")
    return kPauseMouseEventType;
  return kPressedMouseEventType;
}

MouseButton StringToMouseButton(std::string button_type) {
  if (button_type == "left")
    return kLeftMouseButton;
  if (button_type == "middle")
    return kMiddleMouseButton;
  if (button_type == "right")
    return kRightMouseButton;
  if (button_type == "back")
    return kBackMouseButton;
  if (button_type == "forward")
    return kForwardMouseButton;
  return kNoneMouseButton;
}

TouchEventType StringToTouchEventType(std::string action_type) {
  if (action_type == "pointerDown")
    return kTouchStart;
  if (action_type == "pointerUp")
    return kTouchEnd;
  if (action_type == "pointerMove")
    return kTouchMove;
  if (action_type == "pointerCancel")
    return kTouchCancel;
  if (action_type == "pause")
    return kPause;
  return kTouchStart;
}

int StringToModifierMouseButton(std::string button_type) {
  if (button_type == "left")
    return 1;
  if (button_type == "right")
    return 2;
  if (button_type == "middle")
    return 4;
  if (button_type == "back")
    return 8;
  if (button_type == "forward")
    return 16;
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
  if (key == "Shift")
    return kShiftKeyModifierMask;
  if (key == "Control")
    return kControlKeyModifierMask;
  if (key == "Alt")
    return kAltKeyModifierMask;
  if (key == "Meta")
    return kMetaKeyModifierMask;
  return 0;
}

PointerType StringToPointerType(std::string pointer_type) {
  CHECK(pointer_type == "pen" || pointer_type == "mouse");
  if (pointer_type == "pen")
    return kPen;
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

base::Value::Dict CreateDictionaryFrom(const Cookie& cookie) {
  base::Value::Dict dict;
  dict.Set("name", cookie.name);
  dict.Set("value", cookie.value);
  if (!cookie.domain.empty())
    dict.Set("domain", cookie.domain);
  if (!cookie.path.empty())
    dict.Set("path", cookie.path);
  if (!cookie.session)
    SetSafeInt(dict, "expiry", cookie.expiry);
  dict.Set("httpOnly", cookie.http_only);
  dict.Set("secure", cookie.secure);
  if (!cookie.samesite.empty()) {
    dict.Set("sameSite", cookie.samesite);
  } else {
    // The default in the standard is Lax:
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Set-Cookie/SameSite
    // Chrome (mostly) treats default cookies as Lax so this seems correct:
    // https://chromestatus.com/feature/5088147346030592
    dict.Set("sameSite", "Lax");
  }
  return dict;
}

Status GetVisibleCookies(Session* for_session,
                         WebView* web_view,
                         std::list<Cookie>* cookies) {
  std::string current_page_url;
  Status status =
      GetUrl(web_view, for_session->GetCurrentFrameId(), &current_page_url);
  if (status.IsError())
    return status;
  base::Value internal_cookies;
  status = web_view->GetCookies(&internal_cookies, current_page_url);
  if (status.IsError())
    return status;
  std::list<Cookie> cookies_tmp;
  for (const base::Value& cookie_value : internal_cookies.GetList()) {
    if (!cookie_value.is_dict())
      return Status(kUnknownError, "DevTools returns a non-dictionary cookie");

    const base::Value::Dict& cookie_dict = cookie_value.GetDict();

    const std::string* name = cookie_dict.FindString("name");
    const std::string* value = cookie_dict.FindString("value");
    const std::string* domain = cookie_dict.FindString("domain");
    const std::string* path = cookie_dict.FindString("path");
    std::string samesite;
    GetOptionalString(cookie_dict, "sameSite", &samesite);
    int64_t expiry =
        static_cast<int64_t>(cookie_dict.FindDouble("expires").value_or(0));
    // Truncate & convert the value to an integer as required by W3C spec.
    if (expiry >= (1ll << 53) || expiry <= -(1ll << 53))
      expiry = 0;
    bool http_only = cookie_dict.FindBool("httpOnly").value_or(false);
    bool session = cookie_dict.FindBool("session").value_or(false);
    bool secure = cookie_dict.FindBool("secure").value_or(false);

    cookies_tmp.push_back(Cookie(*name, *value, *domain, *path, samesite,
                                 expiry, http_only, secure, session));
  }
  cookies->swap(cookies_tmp);
  return Status(kOk);
}

Status ScrollCoordinateInToView(
    Session* session, WebView* web_view, int x, int y, int* offset_x,
    int* offset_y) {
  std::unique_ptr<base::Value> value;
  base::Value::List args;
  args.Append(x);
  args.Append(y);
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
  const base::Value::Dict& view_attrib = value->GetDict();
  int view_x = view_attrib.FindInt("view_x").value_or(0);
  int view_y = view_attrib.FindInt("view_y").value_or(0);
  int view_width = view_attrib.FindInt("view_width").value_or(0);
  int view_height = view_attrib.FindInt("view_height").value_or(0);
  *offset_x = x - view_x;
  *offset_y = y - view_y;
  if (*offset_x < 0 || *offset_x >= view_width || *offset_y < 0 ||
      *offset_y >= view_height) {
    return Status(kUnknownError, "Failed to scroll coordinate into view");
  }
  return Status(kOk);
}

Status ExecuteTouchEvent(Session* session,
                         WebView* web_view,
                         TouchEventType type,
                         const base::Value::Dict& params) {
  std::optional<int> x = params.FindInt("x");
  std::optional<int> y = params.FindInt("y");
  if (!x)
    return Status(kInvalidArgument, "'x' must be an integer");
  if (!y)
    return Status(kInvalidArgument, "'y' must be an integer");
  int relative_x = *x;
  int relative_y = *y;
  Status status = ScrollCoordinateInToView(session, web_view, *x, *y,
                                           &relative_x, &relative_y);
  if (!status.IsOk())
    return status;
  std::vector<TouchEvent> events;
  events.emplace_back(type, relative_x, relative_y);
  return web_view->DispatchTouchEvents(events, false);
}

Status WindowViewportSize(Session* session,
                          WebView* web_view,
                          int* inner_width,
                          int* inner_height) {
  DCHECK(inner_width);
  DCHECK(inner_height);
  std::unique_ptr<base::Value> value;
  base::Value::List args;
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
  const base::Value::Dict& view_attrib = value->GetDict();
  std::optional<int> maybe_inner_width = view_attrib.FindInt("view_width");
  if (maybe_inner_width)
    *inner_width = *maybe_inner_width;

  std::optional<int> maybe_inner_height = view_attrib.FindInt("view_height");
  if (maybe_inner_height)
    *inner_height = *maybe_inner_height;
  return Status(kOk);
}

Status ProcessPauseAction(const base::Value::Dict& action_item,
                          base::Value::Dict* action) {
  int duration = 0;
  bool has_value = false;
  if (!GetOptionalInt(action_item, "duration", &duration, &has_value) ||
      duration < 0)
    return Status(kInvalidArgument, "'duration' must be a non-negative int");
  if (has_value)
    action->Set("duration", duration);
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

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  // On Mac and Windows, we keep increasing the click count, but on the other
  // platforms, we reset the count to 1 when it is greater than 3.
  if (last_click_count >= 3)
    return 1;
#endif
  return last_click_count + 1;
}

const char kLandscape[] = "landscape";
const char kPortrait[] = "portrait";

Status ParseOrientation(const base::Value::Dict& params,
                        std::string* orientation) {
  bool has_value;
  if (!GetOptionalString(params, "orientation", orientation, &has_value)) {
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

Status ParseScale(const base::Value::Dict& params, double* scale) {
  bool has_value;
  if (!GetOptionalDouble(params, "scale", scale, &has_value)) {
    return Status(kInvalidArgument, "'scale' must be a double");
  }

  if (!has_value) {
    *scale = 1;
  } else if (*scale < 0.1 || *scale > 2) {
    return Status(kInvalidArgument, "'scale' must not be < 0.1 or > 2");
  }
  return Status(kOk);
}

Status ParseBoolean(const base::Value::Dict& params,
                    const std::string& name,
                    bool default_value,
                    bool* b) {
  *b = default_value;
  if (!GetOptionalBool(params, name, b)) {
    return Status(kInvalidArgument, "'" + name + "' must be a boolean");
  }
  return Status(kOk);
}

Status GetNonNegativeDouble(const base::Value::Dict& dict,
                            const std::string& parent,
                            const std::string& child,
                            double* attribute) {
  bool has_value;
  std::string attribute_str = "'" + parent + "." + child + "'";
  if (!GetOptionalDouble(dict, child, attribute, &has_value)) {
    return Status(kInvalidArgument, attribute_str + " must be a double");
  }

  if (has_value) {
    *attribute = ConvertCentimeterToInch(*attribute);
    if (*attribute < 0) {
      return Status(kInvalidArgument,
                    attribute_str + " must not be less than 0");
    }
  }
  return Status(kOk);
}

struct Page {
  double width;
  double height;
};

Status ParsePage(const base::Value::Dict& params, Page* page) {
  bool has_value;
  const base::Value::Dict* page_dict = nullptr;
  if (!GetOptionalDictionary(params, "page", &page_dict, &has_value)) {
    return Status(kInvalidArgument, "'page' must be an object");
  }
  page->width = ConvertCentimeterToInch(21.59);
  page->height = ConvertCentimeterToInch(27.94);
  if (!has_value)
    return Status(kOk);

  Status status =
      GetNonNegativeDouble(*page_dict, "page", "width", &page->width);
  if (status.IsError())
    return status;

  status = GetNonNegativeDouble(*page_dict, "page", "height", &page->height);
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

Status ParseMargin(const base::Value::Dict& params, Margin* margin) {
  bool has_value;
  const base::Value::Dict* margin_dict = nullptr;
  if (!GetOptionalDictionary(params, "margin", &margin_dict, &has_value)) {
    return Status(kInvalidArgument, "'margin' must be an object");
  }

  margin->top = ConvertCentimeterToInch(1.0);
  margin->bottom = ConvertCentimeterToInch(1.0);
  margin->left = ConvertCentimeterToInch(1.0);
  margin->right = ConvertCentimeterToInch(1.0);

  if (!has_value)
    return Status(kOk);

  Status status =
      GetNonNegativeDouble(*margin_dict, "margin", "top", &margin->top);
  if (status.IsError())
    return status;

  status =
      GetNonNegativeDouble(*margin_dict, "margin", "bottom", &margin->bottom);
  if (status.IsError())
    return status;

  status = GetNonNegativeDouble(*margin_dict, "margin", "left", &margin->left);
  if (status.IsError())
    return status;

  status =
      GetNonNegativeDouble(*margin_dict, "margin", "right", &margin->right);
  if (status.IsError())
    return status;

  return Status(kOk);
}

Status ParsePageRanges(const base::Value::Dict& params,
                       std::string* page_ranges) {
  bool has_value;
  const base::Value::List* page_range_list = nullptr;
  if (!GetOptionalList(params, "pageRanges", &page_range_list, &has_value)) {
    return Status(kInvalidArgument, "'pageRanges' must be an array");
  }

  if (!has_value) {
    return Status(kOk);
  }

  std::vector<std::string> ranges;
  for (const base::Value& page_range : *page_range_list) {
    if (page_range.is_int()) {
      if (page_range.GetInt() < 0) {
        return Status(kInvalidArgument,
                      "a Number entry in 'pageRanges' must not be less than 0");
      }
      ranges.push_back(base::NumberToString(page_range.GetInt()));
    } else if (page_range.is_string()) {
      ranges.push_back(page_range.GetString());
    } else {
      return Status(kInvalidArgument,
                    "an entry in 'pageRanges' must be a Number or String");
    }
  }

  *page_ranges = base::JoinString(ranges, ",");
  return Status(kOk);
}

// Returns:
// 1. Optional with the default value, if there is no such a key in the
//    dictionary.
// 2. Empty optional, if the key is in the dictionary, but value has
//    unexpected type.
// 3. Optional with value from dictionary.
template <typename T>
std::optional<T> ParseIfInDictionary(
    const base::Value::Dict& dict,
    std::string_view key,
    T default_value,
    std::optional<T> (base::Value::*getterIfType)() const) {
  const auto* val = dict.Find(key);
  if (!val)
    return std::make_optional(default_value);
  return (val->*getterIfType)();
}

std::optional<double> ParseDoubleIfInDictionary(const base::Value::Dict& dict,
                                                std::string_view key,
                                                double default_value) {
  return ParseIfInDictionary(dict, key, default_value,
                             &base::Value::GetIfDouble);
}

std::optional<int> ParseIntIfInDictionary(const base::Value::Dict& dict,
                                          std::string_view key,
                                          int default_value) {
  return ParseIfInDictionary(dict, key, default_value, &base::Value::GetIfInt);
}
}  // namespace

Status ExecuteWindowCommand(const WindowCommand& command,
                            Session* session,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value) {
  Timeout timeout;
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;

  if (web_view->IsDialogOpen()) {
    std::string alert_text;
    status = web_view->GetDialogMessage(alert_text);
    if (status.IsError())
      return status;

    std::string dialog_type;
    status = web_view->GetTypeOfDialog(dialog_type);
    if (status.IsError()) {
      return status;
    }

    PromptHandlerConfiguration prompt_handler_configuration;
    status = session->unhandled_prompt_behavior.GetConfiguration(
        dialog_type, prompt_handler_configuration);
    if (status.IsError()) {
      return status;
    }

    if (prompt_handler_configuration.type == PromptHandlerType::kAccept ||
        prompt_handler_configuration.type == PromptHandlerType::kDismiss) {
      status = web_view->HandleDialog(
          prompt_handler_configuration.type == PromptHandlerType::kAccept,
          session->prompt_text);
      if (status.IsError()) {
        return status;
      }
    }

    if (prompt_handler_configuration.notify) {
      return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
    }
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
    // Impossible errors:
    // * kNoSuchExecutionContext as WebView::WaitForPendingNavigations never
    //   returns it.
    // Some possible errors:
    // * kTimeout. The pending navigation has taken too long, the whole command
    //   has timed out.
    // * kDisconnected. The connection was lost. There is no point to retry.
    if (nav_status.IsError()) {
      return nav_status;
    }

    status = command.Run(session, web_view, params, value, &timeout);
    if (status.code() == kNoSuchExecutionContext) {
      // Navigation was detected while running the command. Retry.
      continue;
    }
    if (status.code() == kTimeout) {
      // If the command timed out, let WaitForPendingNavigations cancel
      // the navigation if there is one.
      continue;
    } else if (status.code() == kUnknownError && web_view->IsNonBlocking() &&
               status.message().find(kTargetClosedMessage) !=
                   std::string::npos) {
      // When pageload strategy is None, new navigation can occur during
      // execution of a command. Retry the command.
      continue;
    } else if (status.code() == kDisconnected) {
      // Some commands, like clicking a button or link which closes the window,
      // may result in a kDisconnected error code. |web_view| may be invalid at
      // at this point.
      return status;
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
      nav_status.code() != kUnexpectedAlertOpen) {
    return nav_status;
  }
  if (status.code() == kUnexpectedAlertOpen) {
    return Status(kOk);
  }
  if (status.code() == kUnexpectedAlertOpen_Keep) {
    return Status(kUnexpectedAlertOpen, status.message());
  }
  if (status.code() == kNoSuchExecutionContext) {
    // The command has failed to run due to pending navigation three times.
    // Giving up with an appropriate standard error.
    return Status{kUnknownError, status};
  }
  return status;
}

Status ExecuteGet(Session* session,
                  WebView* web_view,
                  const base::Value::Dict& params,
                  std::unique_ptr<base::Value>* value,
                  Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  const std::string* url = params.FindString("url");
  if (!url)
    return Status(kInvalidArgument, "'url' must be a string");
  Status status = web_view->Load(*url, timeout);
  if (status.IsError())
    return status;
  session->SwitchToTopFrame();
  return Status(kOk);
}

Status ExecuteExecuteScript(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const std::string* maybe_script = params.FindString("script");
  if (!maybe_script)
    return Status(kInvalidArgument, "'script' must be a string");
  std::string script = *maybe_script;
  if (script == ":takeHeapSnapshot")
    return web_view->TakeHeapSnapshot(value);
  if (script == ":startProfile")
    return web_view->StartProfile();
  if (script == ":endProfile")
    return web_view->EndProfile(value);

  const base::Value::List* args = params.FindList("args");
  if (args == nullptr) {
    return Status(kInvalidArgument, "'args' must be a list");
  }
  // Need to support line oriented comment
  if (script.find("//") != std::string::npos)
    script = script + "\n";

  Status status =
      web_view->CallUserSyncScript(session->GetCurrentFrameId(), script, *args,
                                   session->script_timeout, value);
  switch (status.code()) {
    case kTimeout:
    // Navigation has happened during script execution. Further wait would lead
    // to timeout.
    case kNavigationDetectedByRemoteEnd:
      return Status(kScriptTimeout);
    default:
      return status;
  }
}

Status ExecuteExecuteAsyncScript(Session* session,
                                 WebView* web_view,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value,
                                 Timeout* timeout) {
  const std::string* maybe_script = params.FindString("script");
  if (!maybe_script)
    return Status(kInvalidArgument, "'script' must be a string");
  std::string script = *maybe_script;
  const base::Value::List* args = params.FindList("args");
  if (args == nullptr) {
    return Status(kInvalidArgument, "'args' must be a list");
  }

  // Need to support line oriented comment
  if (script.find("//") != std::string::npos)
    script = script + "\n";

  Status status = web_view->CallUserAsyncFunction(
      session->GetCurrentFrameId(), "async function(){" + script + "}", *args,
      session->script_timeout, value);
  switch (status.code()) {
    case kTimeout:
    // Navigation has happened during script execution. Further wait would lead
    // to timeout.
    case kNavigationDetectedByRemoteEnd:
      return Status(kScriptTimeout);
    default:
      return status;
  }
}

Status ExecuteNewWindow(Session* session,
                        WebView* web_view,
                        const base::Value::Dict& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  std::string type;
  // "type" can either be None or a string.
  auto* type_param = params.Find("type");
  if (!type_param || type_param->is_none()) {
    // Nothing more to do
  } else if (type_param->is_string()) {
    type = type_param->GetString();
  } else {
    return Status(kInvalidArgument, "missing or invalid 'type'");
  }

  // By default, creates new tab.
  Chrome::WindowType window_type = (type == "window")
                                       ? Chrome::WindowType::kWindow
                                       : Chrome::WindowType::kTab;

  std::string handle;
  Status status =
      session->chrome->NewWindow(session->window, window_type, true, &handle);

  if (status.IsError())
    return status;

  base::Value::Dict dict;
  dict.Set("handle", handle);
  dict.Set("type",
           (window_type == Chrome::WindowType::kWindow) ? "window" : "tab");
  auto results = std::make_unique<base::Value>(std::move(dict));
  *value = std::move(results);
  return Status(kOk);
}

Status ExecuteSwitchToFrame(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const base::Value* id = params.Find("id");
  if (id == nullptr)
    return Status(kInvalidArgument, "missing 'id'");

  if (id->is_none()) {
    session->SwitchToTopFrame();
    return Status(kOk);
  }

  std::string script;
  base::Value::List args;
  const base::Value::Dict* id_dict = id->GetIfDict();
  if (id_dict) {
    const std::string* element_id = id_dict->FindString(GetElementKey());
    if (!element_id)
      return Status(kInvalidArgument, "missing 'ELEMENT'");
    bool is_displayed = false;
    Status status =
        IsElementDisplayed(session, web_view, *element_id, true, &is_displayed);
    if (status.IsError())
      return status;
    script = "function(elem) { return elem; }";
    args.Append(id_dict->Clone());
  } else {
    script =
        "function(xpath) {"
        "  return document.evaluate(xpath, document, null, "
        "      XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;"
        "}";
    std::string xpath = "(/html/body//iframe|/html/frameset//frame)";
    if (id->is_string()) {
      std::string id_string = id->GetString();
      if (session->w3c_compliant)
        return Status(kInvalidArgument, "'id' can not be string");
      else
        xpath += base::StringPrintf(
          "[@name=\"%s\" or @id=\"%s\"]", id_string.c_str(), id_string.c_str());
    } else if (id->is_int()) {
      int id_int = id->GetInt();
      const int max_range = 65535;  // 2^16 - 1
      if (id_int < 0 || id_int > max_range)
        return Status(kInvalidArgument, "'id' out of range");
      else
        xpath += base::StringPrintf("[%d]", id_int + 1);
    } else {
      return Status(kInvalidArgument, "invalid 'id'");
    }
    args.Append(xpath);
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
  const base::Value::Dict* element = result->GetIfDict();
  if (!element)
    return Status(kUnknownError, "fail to locate the sub frame element");

  std::string chrome_driver_id = GenerateId();
  const char kSetFrameIdentifier[] =
      "function(frame, id) {"
      "  frame.setAttribute('cd_frame_id_', id);"
      "}";
  base::Value::List new_args;
  new_args.Append(element->Clone());
  new_args.Append(chrome_driver_id);
  result.reset();
  status = web_view->CallFunction(
      session->GetCurrentFrameId(), kSetFrameIdentifier, new_args, &result);
  if (status.IsError())
    return status;
  session->SwitchToSubFrame(frame, chrome_driver_id);
  return Status(kOk);
}

Status ExecuteSwitchToParentFrame(Session* session,
                                  WebView* web_view,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value,
                                  Timeout* timeout) {
  session->SwitchToParentFrame();
  return Status(kOk);
}

Status ExecuteGetTitle(Session* session,
                       WebView* web_view,
                       const base::Value::Dict& params,
                       std::unique_ptr<base::Value>* value,
                       Timeout* timeout) {
  const char kGetTitleScript[] = "function() {  return document.title;}";
  base::Value::List args;
  return web_view->CallFunction(std::string(), kGetTitleScript, args, value);
}

Status ExecuteGetPageSource(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const char kGetPageSource[] =
      "(document.documentElement || {}).outerHTML || ''";

  base::Value::List args;
  return web_view->EvaluateScript(session->GetCurrentFrameId(), kGetPageSource,
                                  true, value);
}

Status ExecuteFindElement(int interval_ms,
                          Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  return FindElement(interval_ms, true, nullptr, session, web_view, params,
                     value);
}

Status ExecuteFindElements(int interval_ms,
                           Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return FindElement(interval_ms, false, nullptr, session, web_view, params,
                     value);
}

Status ExecuteGetCurrentUrl(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
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
                     const base::Value::Dict& params,
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
                        const base::Value::Dict& params,
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
                      const base::Value::Dict& params,
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
                     const base::Value::Dict& params,
                     std::unique_ptr<base::Value>* value,
                     Timeout* timeout) {
  timeout->SetDuration(session->page_load_timeout);
  Status status = web_view->Freeze(timeout);
  return status;
}

Status ExecuteResume(Session* session,
                     WebView* web_view,
                     const base::Value::Dict& params,
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
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  std::string element_id;
  bool has_element = false;
  const std::string* maybe_element_id = params.FindString("element");
  if (maybe_element_id) {
    element_id = *maybe_element_id;
    has_element = true;
  }
  std::optional<int> x_offset = params.FindInt("xoffset");
  std::optional<int> y_offset = params.FindInt("yoffset");
  bool has_offset = x_offset.has_value() && y_offset.has_value();
  if (!has_element && !has_offset)
    return Status(kInvalidArgument,
                  "at least an element or offset should be set");

  WebPoint location;
  if (has_element) {
    WebPoint offset;
    if (has_offset)
      offset.Offset(*x_offset, *y_offset);
    Status status = ScrollElementIntoView(session, web_view, element_id,
        has_offset ? &offset : nullptr, &location);
    if (status.IsError())
      return status;
  } else {
    location = session->mouse_position;
    if (has_offset)
      location.Offset(*x_offset, *y_offset);
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
                         const base::Value::Dict& params,
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
                              const base::Value::Dict& params,
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
                            const base::Value::Dict& params,
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
                               const base::Value::Dict& params,
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
                        const base::Value::Dict& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchStart, params);
}

Status ExecuteTouchUp(Session* session,
                      WebView* web_view,
                      const base::Value::Dict& params,
                      std::unique_ptr<base::Value>* value,
                      Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchEnd, params);
}

Status ExecuteTouchMove(Session* session,
                        WebView* web_view,
                        const base::Value::Dict& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  return ExecuteTouchEvent(session, web_view, kTouchMove, params);
}

Status ExecuteTouchScroll(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  WebPoint location = session->mouse_position;
  const std::string* element = params.FindString("element");
  if (element) {
    Status status =
        GetElementClickableLocation(session, web_view, *element, &location);
    if (status.IsError())
      return status;
  }
  std::optional<int> xoffset = params.FindInt("xoffset");
  if (!xoffset)
    return Status(kInvalidArgument, "'xoffset' must be an integer");
  std::optional<int> yoffset = params.FindInt("yoffset");
  if (!yoffset)
    return Status(kInvalidArgument, "'yoffset' must be an integer");
  return web_view->SynthesizeScrollGesture(location.x, location.y, *xoffset,
                                           *yoffset);
}

Status ProcessInputActionSequence(Session* session,
                                  const base::Value::Dict& action_sequence,
                                  std::vector<base::Value::Dict>* action_list) {
  const std::string* maybe_type = action_sequence.FindString("type");
  std::string pointer_type;
  if (!maybe_type || ((*maybe_type != "key") && (*maybe_type != "pointer") &&
                      (*maybe_type != "wheel") && (*maybe_type != "none"))) {
    return Status(kInvalidArgument,
                  "'type' must be one of the strings 'key', 'pointer', 'wheel' "
                  "or 'none'");
  }
  const std::string& type = *maybe_type;

  const std::string* maybe_id = action_sequence.FindString("id");
  if (!maybe_id)
    return Status(kInvalidArgument, "'id' must be a string");
  const std::string& id = *maybe_id;

  if (type == "pointer") {
    const base::Value::Dict* parameters =
        action_sequence.FindDict("parameters");
    if (parameters) {
      const std::string* maybe_pointer_type =
          parameters->FindString("pointerType");
      // error check arguments
      if (!maybe_pointer_type ||
          (*maybe_pointer_type != "mouse" && *maybe_pointer_type != "pen" &&
           *maybe_pointer_type != "touch")) {
        return Status(
            kInvalidArgument,
            "'pointerType' must be a string and one of mouse, pen or touch");
      }
      pointer_type = *maybe_pointer_type;
    } else {
      pointer_type = "mouse";
    }
  }

  bool found = false;
  for (const base::Value& source_value : session->active_input_sources) {
    DCHECK(source_value.is_dict());
    const base::Value::Dict& source = source_value.GetDict();

    std::string source_id;
    std::string source_type;
    const std::string* maybe_source_id = source.FindString("id");
    const std::string* maybe_source_type = source.FindString("type");
    if (maybe_source_id)
      source_id = *maybe_source_id;
    if (maybe_source_type)
      source_type = *maybe_source_type;
    if (source_id == id && source_type == type) {
      found = true;
      if (type == "pointer") {
        const std::string* source_pointer_type =
            source.FindString("pointerType");
        if (!source_pointer_type || pointer_type != *source_pointer_type) {
          return Status(kInvalidArgument,
                        "'pointerType' must be a string that matches sources "
                        "pointer type");
        }
      }
      break;
    }
  }

  // if we found no matching active input source
  if (!found) {
    base::Value::Dict tmp_source;
    // create input source
    tmp_source.Set("id", id);
    tmp_source.Set("type", type);
    if (type == "pointer") {
      tmp_source.Set("pointerType", pointer_type);
    }

    session->active_input_sources.Append(std::move(tmp_source));

    base::Value::Dict tmp_state;
    tmp_state.Set("id", id);
    if (type == "key") {
      // Initialize a key input state object
      // (https://w3c.github.io/webdriver/#dfn-key-input-state).
      tmp_state.Set("pressed", base::Value::Dict());
      // For convenience, we use one integer property to encode four Boolean
      // properties (alt, shift, ctrl, meta) from the spec, using values from
      // enum KeyModifierMask.
      tmp_state.Set("modifiers", 0);
    } else if (type == "pointer") {
      int x = 0;
      int y = 0;

      // "pressed" is stored as a bitmask of pointer buttons.
      tmp_state.Set("pressed", 0);
      tmp_state.Set("subtype", pointer_type);

      tmp_state.Set("x", x);
      tmp_state.Set("y", y);
    }
    session->input_state_table.SetByDottedPath(id, std::move(tmp_state));
  }

  const base::Value::List* actions = action_sequence.FindList("actions");
  if (actions == nullptr) {
    return Status(kInvalidArgument, "'actions' in the sequence must be a list");
  }

  std::unique_ptr<base::Value::List> actions_result(new base::Value::List);
  for (const base::Value& action_item_value : *actions) {
    base::Value::Dict action_dict;

    if (!action_item_value.is_dict()) {
      return Status(
          kInvalidArgument,
          "each argument in the action sequence must be a dictionary");
    }

    const base::Value::Dict& action_item = action_item_value.GetDict();

    action_dict.Set("id", id);
    action_dict.Set("type", type);

    if (type == "none") {
      // process none action
      const std::string* subtype = action_item.FindString("type");
      if (!subtype || *subtype != "pause")
        return Status(kInvalidArgument,
                      "type of action must be the  string 'pause'");

      action_dict.Set("subtype", *subtype);

      Status status = ProcessPauseAction(action_item, &action_dict);
      if (status.IsError())
        return status;
    } else if (type == "key") {
      // process key action
      const std::string* subtype = action_item.FindString("type");
      if (!subtype ||
          (*subtype != "keyUp" && *subtype != "keyDown" && *subtype != "pause"))
        return Status(
            kInvalidArgument,
            "type of action must be the string 'keyUp', 'keyDown' or 'pause'");

      action_dict.Set("subtype", *subtype);
      if (*subtype == "pause") {
        Status status = ProcessPauseAction(action_item, &action_dict);
        if (status.IsError())
          return status;
      } else {
        const std::string* key = action_item.FindString("value");
        bool valid = (key != nullptr);
        if (valid) {
          // check if key is a single unicode code point
          size_t char_index = 0;
          base_icu::UChar32 code_point;
          valid = base::ReadUnicodeCharacter(key->c_str(), key->size(),
                                             &char_index, &code_point) &&
                  char_index + 1 == key->size();
        }
        if (!valid)
          return Status(kInvalidArgument,
                        "'value' must be a single Unicode code point");
        action_dict.Set("value", *key);
      }
    } else if (type == "pointer" || type == "wheel") {
      const std::string* subtype = action_item.FindString("type");
      if (type == "pointer") {
        if (!subtype || (*subtype != "pointerUp" && *subtype != "pointerDown" &&
                         *subtype != "pointerMove" &&
                         *subtype != "pointerCancel" && *subtype != "pause")) {
          return Status(kInvalidArgument,
                        "type of pointer action must be the string "
                        "'pointerUp', 'pointerDown', 'pointerMove' or "
                        "'pause'");
        }
      } else {
        if (!subtype || (*subtype != "scroll" && *subtype != "pause")) {
          return Status(
              kInvalidArgument,
              "type of action must be the string 'scroll' or 'pause'");
        }
      }

      action_dict.Set("subtype", *subtype);
      action_dict.Set("pointerType", pointer_type);

      if (*subtype == "pointerDown" || *subtype == "pointerUp") {
        if (pointer_type == "mouse" || pointer_type == "pen") {
          int button = action_item.FindInt("button").value_or(-1);
          if (button < 0 || button > 4) {
            return Status(
                kInvalidArgument,
                "'button' must be a non-negative int and between 0 and 4");
          }
          std::string button_str;
          Status status = IntToStringButton(button, button_str);
          if (status.IsError())
            return status;
          action_dict.Set("button", button_str);
        }
      } else if (*subtype == "pointerMove" || *subtype == "scroll") {
        std::optional<int> x = action_item.FindInt("x");
        if (!x.has_value())
          return Status(kInvalidArgument, "'x' must be an int");
        std::optional<int> y = action_item.FindInt("y");
        if (!y.has_value())
          return Status(kInvalidArgument, "'y' must be an int");
        action_dict.Set("x", *x);
        action_dict.Set("y", *y);

        const base::Value* origin_val = action_item.Find("origin");
        if (origin_val) {
          if (!origin_val->is_string()) {
            const base::Value::Dict* origin_dict = origin_val->GetIfDict();
            if (!origin_dict)
              return Status(kInvalidArgument,
                            "'origin' must be either a string or a dictionary");
            const std::string* element_id =
                origin_dict->FindString(GetElementKey());
            if (!element_id)
              return Status(kInvalidArgument, "'element' is missing");
            base::Value* origin_result =
                action_dict.Set("origin", base::Value(base::Value::Type::DICT));
            origin_result->GetDict().SetByDottedPath(GetElementKey(),
                                                     *element_id);
          } else {
            const std::string& origin = origin_val->GetString();
            if (origin != "viewport" && origin != "pointer")
              return Status(kInvalidArgument,
                            "if 'origin' is a string, it must be either "
                            "'viewport' or 'pointer'");
            action_dict.Set("origin", origin);
          }
        } else {
          action_dict.Set("origin", "viewport");
        }

        Status status = ProcessPauseAction(action_item, &action_dict);
        if (status.IsError())
          return status;

        if (*subtype == "scroll") {
          std::optional<int> delta_x = action_item.FindInt("deltaX");
          if (!delta_x)
            return Status(kInvalidArgument, "'delta x' must be an int");
          std::optional<int> delta_y = action_item.FindInt("deltaY");
          if (!delta_y)
            return Status(kInvalidArgument, "'delta y' must be an int");
          action_dict.Set("deltaX", *delta_x);
          action_dict.Set("deltaY", *delta_y);
        }
      } else if (*subtype == "pause") {
        Status status = ProcessPauseAction(action_item, &action_dict);
        if (status.IsError())
          return status;
      }

      // Process Pointer Event's properties.
      std::optional<double> maybe_double_value;
      std::optional<int> maybe_int_value;

      maybe_double_value = ParseDoubleIfInDictionary(action_item, "width", 1);
      if (!maybe_double_value.has_value() || maybe_double_value.value() < 0)
        return Status(kInvalidArgument,
                      "'width' must be a non-negative number");
      action_dict.Set("width", maybe_double_value.value());

      maybe_double_value = ParseDoubleIfInDictionary(action_item, "height", 1);
      if (!maybe_double_value.has_value() || maybe_double_value.value() < 0)
        return Status(kInvalidArgument,
                      "'height' must be a non-negative number");
      action_dict.Set("height", maybe_double_value.value());

      maybe_double_value =
          ParseDoubleIfInDictionary(action_item, "pressure", 0.5);
      if (!maybe_double_value.has_value() || maybe_double_value.value() < 0 ||
          maybe_double_value.value() > 1)
        return Status(
            kInvalidArgument,
            "'pressure' must be a non-negative number in the range of [0,1]");
      action_dict.Set("pressure", maybe_double_value.value());

      maybe_double_value =
          ParseDoubleIfInDictionary(action_item, "tangentialPressure", 0);
      if (!maybe_double_value.has_value() || maybe_double_value.value() < -1 ||
          maybe_double_value.value() > 1)
        return Status(
            kInvalidArgument,
            "'tangentialPressure' must be a number in the range of [-1,1]");
      action_dict.Set("tangentialPressure", maybe_double_value.value());

      maybe_int_value = ParseIntIfInDictionary(action_item, "tiltX", 0);
      if (!maybe_int_value.has_value() || maybe_int_value.value() < -90 ||
          maybe_int_value.value() > 90)
        return Status(kInvalidArgument,
                      "'tiltX' must be an integer in the range of [-90,90]");
      action_dict.Set("tiltX", maybe_int_value.value());

      maybe_int_value = ParseIntIfInDictionary(action_item, "tiltY", 0);
      if (!maybe_int_value.has_value() || maybe_int_value.value() < -90 ||
          maybe_int_value.value() > 90)
        return Status(kInvalidArgument,
                      "'tiltY' must be an integer in the range of [-90,90]");
      action_dict.Set("tiltY", maybe_int_value.value());

      maybe_int_value = ParseIntIfInDictionary(action_item, "twist", 0);
      if (!maybe_int_value.has_value() || maybe_int_value.value() < 0 ||
          maybe_int_value.value() > 359)
        return Status(kInvalidArgument,
                      "'twist' must be an integer in the range of [0,359]");
      action_dict.Set("twist", maybe_int_value.value());
    }
    action_list->push_back(std::move(action_dict));
  }
  return Status(kOk);
}

Status ExecutePerformActions(Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  // extract action sequence
  const base::Value::List* actions_input = params.FindList("actions");
  if (actions_input == nullptr) {
    return Status(kInvalidArgument, "'actions' must be a list");
  }

  // the processed actions
  std::vector<std::vector<base::Value::Dict>> actions_list;
  for (const base::Value& action_sequence : *actions_input) {
    // process input action sequence
    if (!action_sequence.is_dict())
      return Status(kInvalidArgument, "each argument must be a dictionary");

    std::vector<base::Value::Dict> action_list;
    Status status = ProcessInputActionSequence(
        session, action_sequence.GetDict(), &action_list);
    actions_list.push_back(std::move(action_list));

    if (status.IsError())
      return Status(kInvalidArgument, status);
  }

  std::set<std::string> pointer_id_set;
  std::vector<base::Value::Dict*> action_input_states;
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
        const base::Value::Dict& action = actions_list[j][i];
        std::string type;
        std::string action_type;
        GetOptionalString(action, "type", &type);
        GetOptionalString(action, "subtype", &action_type);
        if (type != "none" && action_type != "pause")
          last_action_index = j;

        if (type == "pointer") {
          std::string pointer_type;
          GetOptionalString(action, "pointerType", &pointer_type);
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
        const base::Value::Dict& action = actions_list[j][i];
        std::string id;
        std::string type;
        std::string action_type;
        GetOptionalString(action, "id", &id);

        base::Value::Dict* input_state =
            session->input_state_table.FindDictByDottedPath(id);
        if (!input_state)
          return Status(kUnknownError, "missing input state");

        GetOptionalString(action, "type", &type);
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
            std::optional<int> maybe_init_x = input_state->FindInt("x");
            if (maybe_init_x)
              init_x = *maybe_init_x;

            std::optional<int> maybe_init_y = input_state->FindInt("y");
            if (maybe_init_y)
              init_y = *maybe_init_y;
            action_locations.insert(
                std::make_pair(id, gfx::Point(init_x, init_y)));

            std::string pointer_type;
            GetOptionalString(action, "pointerType", &pointer_type);
            if (pointer_type == "mouse" || pointer_type == "pen") {
              buttons[id] = input_state->Find("pressed")->GetInt();
              last_pressed_buttons[id] = buttons[id];
            } else if (pointer_type == "touch") {
              has_touch_start[id] = false;
            }
          }
        }

        GetOptionalString(action, "subtype", &action_type);
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
                  action, *input_state, action_type == "keyDown",
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

                status = web_view->DispatchKeyEvents(dispatch_key_events,
                                                     async_dispatch_event);
                if (status.IsError())
                  return status;
              }
            }
          } else if (type == "pointer" || type == "wheel") {
            std::string element_id;
            if (action_type == "pointerMove" || action_type == "scroll") {
              double x = action.FindDouble("x").value_or(0);
              double y = action.FindDouble("y").value_or(0);
              if (const base::Value* origin_val = action.Find("origin")) {
                if (const base::Value::Dict* origin_dict =
                        origin_val->GetIfDict()) {
                  GetOptionalString(*origin_dict, GetElementKey(), &element_id);
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
                  GetOptionalString(action, "origin", &origin_str);
                  if (origin_str == "pointer") {
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
                int delta_x = action.FindInt("deltaX").value_or(0);
                int delta_y = action.FindInt("deltaY").value_or(0);
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

            double width = action.FindDouble("width").value_or(1);
            double height = action.FindDouble("height").value_or(1);
            double pressure = action.FindDouble("pressure").value_or(0.5);
            double tangential_pressure =
                action.FindDouble("tangentialPressure").value_or(0);
            int tilt_x = action.FindInt("tiltX").value_or(0);
            int tilt_y = action.FindInt("tiltY").value_or(0);
            int twist = action.FindInt("twist").value_or(0);

            std::string pointer_type;
            GetOptionalString(action, "pointerType", &pointer_type);
            if (pointer_type == "mouse" || pointer_type == "pen") {
              if (action_type != "pause") {
                std::vector<MouseEvent> dispatch_mouse_events;
                int click_count = 0;
                if (action_type == "pointerDown" ||
                    action_type == "pointerUp") {
                  std::string button;
                  GetOptionalString(action, "button", &button);
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
                  action_input_states[j]->Set(
                      "pressed",
                      action_input_states[j]->Find("pressed")->GetInt() |
                          (1 << event.button));
                } else if (event.type == kReleasedMouseEventType) {
                  pressure = 0;
                  event.click_count = session->click_count;
                  buttons[id] &= ~StringToModifierMouseButton(button_type[id]);
                  action_input_states[j]->Set(
                      "pressed",
                      action_input_states[j]->Find("pressed")->GetInt() &
                          ~(1 << event.button));
                } else if (event.type == kMovedMouseEventType) {
                  if (action_input_states[j]->Find("pressed")->GetInt() == 0) {
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
                action_input_states[j]->Set("pressed", 1);
              } else if (event.type == kTouchEnd) {
                action_input_states[j]->Set("pressed", 0);
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
            action_input_states[j]->Set("x", action_locations[id].x());
            action_input_states[j]->Set("y", action_locations[id].y());
          }
        }
      }
    }

    if (tick_duration > 0) {
      base::PlatformThread::Sleep(base::Milliseconds(tick_duration));
    }
  }

  return Status(kOk);
}

Status ExecuteReleaseActions(Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  for (const InputCancelListEntry& entry :
       base::Reversed(session->input_cancel_list)) {
    if (entry.key_event) {
      base::Value::Dict* pressed = entry.input_state->FindDict("pressed");
      if (!pressed->Find(entry.key_event->key))
        continue;
      web_view->DispatchKeyEvents({*entry.key_event}, false);
      pressed->Remove(entry.key_event->key);
    } else if (entry.mouse_event) {
      int pressed = entry.input_state->Find("pressed")->GetInt();
      int button_mask = 1 << entry.mouse_event->button;
      if ((pressed & button_mask) == 0)
        continue;
      web_view->DispatchMouseEvents({*entry.mouse_event},
                                    session->GetCurrentFrameId(), false);
      entry.input_state->Set("pressed", pressed & ~button_mask);
    } else if (entry.touch_event) {
      int pressed = entry.input_state->Find("pressed")->GetInt();
      if (pressed == 0)
        continue;
      web_view->DispatchTouchEvents({*entry.touch_event}, false);
      entry.input_state->Set("pressed", 0);
    }
  }

  session->input_cancel_list.clear();
  session->input_state_table.clear();
  session->active_input_sources.clear();
  session->mouse_position = WebPoint(0, 0);
  session->click_count = 0;
  session->mouse_click_timestamp = base::TimeTicks::Now();
  session->sticky_modifiers = 0;

  return Status(kOk);
}

Status ExecuteSendCommand(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  const std::string* cmd = params.FindString("cmd");
  if (!cmd) {
    return Status(kInvalidArgument, "command not passed");
  }
  const base::Value::Dict* cmd_params = params.FindDict("params");
  if (!cmd_params) {
    return Status(kInvalidArgument, "params not passed");
  }
  return web_view->SendCommand(*cmd, *cmd_params);
}

Status ExecuteSendCommandFromWebSocket(Session* session,
                                       WebView* web_view,
                                       const base::Value::Dict& params,
                                       std::unique_ptr<base::Value>* value,
                                       Timeout* timeout) {
  const std::string* cmd = params.FindString("method");
  if (!cmd) {
    return Status(kInvalidArgument, "command not passed");
  }
  const base::Value::Dict* cmd_params = params.FindDict("params");
  if (!cmd_params) {
    return Status(kInvalidArgument, "params not passed");
  }
  std::optional<int> client_cmd_id = params.FindInt("id");
  if (!client_cmd_id || !CommandId::IsClientCommandId(*client_cmd_id)) {
    return Status(kInvalidArgument, "command id must be negative");
  }

  return web_view->SendCommandFromWebSocket(*cmd, *cmd_params, *client_cmd_id);
}

Status ExecuteSendCommandAndGetResult(Session* session,
                                      WebView* web_view,
                                      const base::Value::Dict& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  const std::string* cmd = params.FindString("cmd");
  if (!cmd) {
    return Status(kInvalidArgument, "command not passed");
  }
  const base::Value::Dict* cmd_params = params.FindDict("params");
  if (!cmd_params) {
    return Status(kInvalidArgument, "params not passed");
  }
  return web_view->SendCommandAndGetResult(*cmd, *cmd_params, value);
}

Status ExecuteGetActiveElement(Session* session,
                               WebView* web_view,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  return GetActiveElement(session, web_view, value);
}

Status ExecuteSendKeysToActiveElement(Session* session,
                                      WebView* web_view,
                                      const base::Value::Dict& params,
                                      std::unique_ptr<base::Value>* value,
                                      Timeout* timeout) {
  const base::Value::List* key_list = params.FindList("value");
  if (key_list == nullptr) {
    return Status(kInvalidArgument, "'value' must be a list");
  }
  return SendKeysOnWindow(
      web_view, key_list, false, &session->sticky_modifiers);
}

Status ExecuteGetStorageItem(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  const std::string* key = params.FindString("key");
  if (!key)
    return Status(kInvalidArgument, "'key' must be a string");
  base::Value::List args;
  args.Append(*key);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key) { return %s[key]; }", storage),
      std::move(args), value);
}

Status ExecuteGetStorageKeys(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
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
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  const std::string* key = params.FindString("key");
  if (!key)
    return Status(kInvalidArgument, "'key' must be a string");
  const std::string* storage_value = params.FindString("value");
  if (!storage_value)
    return Status(kInvalidArgument, "'value' must be a string");
  base::Value::List args;
  args.Append(*key);
  args.Append(*storage_value);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key, value) { %s[key] = value; }", storage),
      args,
      value);
}

Status ExecuteRemoveStorageItem(const char* storage,
                                Session* session,
                                WebView* web_view,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  const std::string* key = params.FindString("key");
  if (!key)
    return Status(kInvalidArgument, "'key' must be a string");
  base::Value::List args;
  args.Append(*key);
  return web_view->CallFunction(
      session->GetCurrentFrameId(),
      base::StringPrintf("function(key) { %s.removeItem(key) }", storage),
      args,
      value);
}

Status ExecuteClearStorage(const char* storage,
                           Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return web_view->EvaluateScript(session->GetCurrentFrameId(),
                                  base::StringPrintf("%s.clear()", storage),
                                  false, value);
}

Status ExecuteGetStorageSize(const char* storage,
                             Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  return web_view->EvaluateScript(session->GetCurrentFrameId(),
                                  base::StringPrintf("%s.length", storage),
                                  false, value);
}

Status ExecuteScreenshot(Session* session,
                         WebView* web_view,
                         const base::Value::Dict& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  Status status = session->chrome->ActivateWebView(web_view->GetId());
  if (status.IsError())
    return status;

  std::string screenshot;
  status = web_view->CaptureScreenshot(&screenshot, base::Value::Dict());
  if (status.IsError()) {
    if (status.code() == kUnexpectedAlertOpen) {
      LOG(WARNING) << status.message() << ", cancelling screenshot";
      // we can't take screenshot in this state
      // but we must return kUnexpectedAlertOpen_Keep instead
      // see https://crbug.com/chromedriver/2117
      return Status(kUnexpectedAlertOpen_Keep);
    }
    LOG(WARNING) << "screenshot failed, retrying " << status.message();
    status = web_view->CaptureScreenshot(&screenshot, base::Value::Dict());
  }
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(screenshot);
  return Status(kOk);
}

Status ExecuteFullPageScreenshot(Session* session,
                                 WebView* web_view,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value,
                                 Timeout* timeout) {
  Status status = session->chrome->ActivateWebView(web_view->GetId());
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> layout_metrics;
  // TODO(crbug.com/40911917): Pass base::Value::Dict* as return param.
  status = web_view->SendCommandAndGetResult(
      "Page.getLayoutMetrics", base::Value::Dict(), &layout_metrics);
  if (status.IsError())
    return status;

  CHECK(layout_metrics && layout_metrics->is_dict());
  const auto& layout_metrics_dict = layout_metrics->GetDict();
  const auto width =
      layout_metrics_dict.FindDoubleByDottedPath("contentSize.width");
  if (!width.has_value())
    return Status(kUnknownError, "invalid width type");
  int w = ceil(width.value());
  if (w == 0)
    return Status(kUnknownError, "invalid width 0");

  const auto height =
      layout_metrics_dict.FindDoubleByDottedPath("contentSize.height");
  if (!height.has_value())
    return Status(kUnknownError, "invalid height type");
  int h = ceil(height.value());
  if (h == 0)
    return Status(kUnknownError, "invalid height 0");

  auto* meom = web_view->GetMobileEmulationOverrideManager();
  bool has_override_metrics = meom->HasOverrideMetrics();

  base::Value::Dict device_metrics;
  device_metrics.Set("width", w);
  device_metrics.Set("height", h);
  if (has_override_metrics) {
    const auto* dm = meom->GetDeviceMetrics();
    device_metrics.Set("deviceScaleFactor", dm->device_scale_factor);
    device_metrics.Set("mobile", dm->mobile);
  } else {
    device_metrics.Set("deviceScaleFactor", 1);
    device_metrics.Set("mobile", false);
  }
  std::unique_ptr<base::Value> ignore;
  status = web_view->SendCommandAndGetResult(
      "Emulation.setDeviceMetricsOverride", device_metrics, &ignore);
  if (status.IsError())
    return status;

  std::string screenshot;
  // No need to supply clip as it would be default to the device metrics
  // parameters
  status = web_view->CaptureScreenshot(&screenshot, base::Value::Dict());
  if (status.IsError()) {
    if (status.code() == kUnexpectedAlertOpen) {
      LOG(WARNING) << status.message() << ", cancelling screenshot";
      // we can't take screenshot in this state
      // but we must return kUnexpectedAlertOpen_Keep instead
      // see https://crbug.com/chromedriver/2117
      return Status(kUnexpectedAlertOpen_Keep);
    }
    LOG(WARNING) << "screenshot failed, retrying " << status.message();
    status = web_view->CaptureScreenshot(&screenshot, base::Value::Dict());
  }
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(screenshot);

  // Check if there is already deviceMetricsOverride in use,
  // if so, restore to that instead
  if (has_override_metrics) {
    status = meom->RestoreOverrideMetrics();
  } else {
    // The scroll bar disappear after setting device metrics to fullpage
    // width and height, this is to clear device metrics and restore
    // scroll bars
    status = web_view->SendCommandAndGetResult(
        "Emulation.clearDeviceMetricsOverride", base::Value::Dict(), &ignore);
  }
  return status;
}

Status ExecutePrint(Session* session,
                    WebView* web_view,
                    const base::Value::Dict& params,
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

  bool shrink_to_fit;
  status = ParseBoolean(params, "shrinkToFit", true, &shrink_to_fit);
  if (status.IsError())
    return status;

  std::string page_ranges;
  status = ParsePageRanges(params, &page_ranges);
  if (status.IsError())
    return status;

  base::Value::Dict print_params;
  print_params.Set(kLandscape, orientation == kLandscape);
  print_params.Set("scale", scale);
  print_params.Set("printBackground", background);
  print_params.Set("paperWidth", page.width);
  print_params.Set("paperHeight", page.height);
  print_params.Set("marginTop", margin.top);
  print_params.Set("marginBottom", margin.bottom);
  print_params.Set("marginLeft", margin.left);
  print_params.Set("marginRight", margin.right);
  print_params.Set("preferCSSPageSize", !shrink_to_fit);
  print_params.Set("pageRanges", page_ranges);
  print_params.Set("transferMode", "ReturnAsBase64");

  std::string pdf;
  status = web_view->PrintToPDF(print_params, &pdf);
  if (status.IsError())
    return status;

  *value = std::make_unique<base::Value>(pdf);
  return Status(kOk);
}

Status ExecuteGetCookies(Session* session,
                         WebView* web_view,
                         const base::Value::Dict& params,
                         std::unique_ptr<base::Value>* value,
                         Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(session, web_view, &cookies);
  if (status.IsError())
    return status;
  auto cookie_list = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (std::list<Cookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    cookie_list->GetList().Append(CreateDictionaryFrom(*it));
  }
  *value = std::move(cookie_list);
  return Status(kOk);
}

Status ExecuteGetNamedCookie(Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  const std::string* name = params.FindString("name");
  if (!name)
    return Status(kInvalidArgument, "missing 'cookie name'");

  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(session, web_view, &cookies);
  if (status.IsError())
    return status;

  for (std::list<Cookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (*name == it->name) {
      *value =
          base::Value::ToUniquePtrValue(base::Value(CreateDictionaryFrom(*it)));
      return Status(kOk);
    }
  }
  return Status(kNoSuchCookie);
}

Status ExecuteAddCookie(Session* session,
                        WebView* web_view,
                        const base::Value::Dict& params,
                        std::unique_ptr<base::Value>* value,
                        Timeout* timeout) {
  const base::Value::Dict* cookie = params.FindDict("cookie");
  if (!cookie)
    return Status(kInvalidArgument, "missing 'cookie'");
  const std::string* name = cookie->FindString("name");
  const std::string* cookie_value = cookie->FindString("value");
  if (!name)
    return Status(kInvalidArgument, "missing 'name'");
  if (!cookie_value)
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
  if (!GetOptionalString(*cookie, "domain", &domain))
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
  if (!GetOptionalString(*cookie, "path", &path))
    return Status(kInvalidArgument, "invalid 'path'");
  std::string samesite("");
  if (!GetOptionalString(*cookie, "sameSite", &samesite))
    return Status(kInvalidArgument, "invalid 'sameSite'");
  if (!samesite.empty() && samesite != "Strict" && samesite != "Lax" &&
      samesite != "None")
    return Status(kInvalidArgument, "invalid 'sameSite'");
  bool secure = false;
  if (!GetOptionalBool(*cookie, "secure", &secure))
    return Status(kInvalidArgument, "invalid 'secure'");
  bool http_only = false;
  if (!GetOptionalBool(*cookie, "httpOnly", &http_only))
    return Status(kInvalidArgument, "invalid 'httpOnly'");
  double expiry;
  bool has_value;
  if (session->w3c_compliant) {
    // W3C spec says expiry is a safe integer.
    int64_t expiry_int64;
    if (!GetOptionalSafeInt(*cookie, "expiry", &expiry_int64, &has_value) ||
        (has_value && expiry_int64 < 0))
      return Status(kInvalidArgument, "invalid 'expiry'");
    // Use negative value to indicate expiry not specified.
    expiry = has_value ? static_cast<double>(expiry_int64) : -1.0;
  } else {
    // JSON wire protocol didn't specify the type of expiry, but ChromeDriver
    // has always accepted double, so we keep that in legacy mode.
    if (!GetOptionalDouble(*cookie, "expiry", &expiry, &has_value) ||
        (has_value && expiry < 0))
      return Status(kInvalidArgument, "invalid 'expiry'");
    if (!has_value)
      expiry = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds() +
               kDefaultCookieExpiryTime;
  }
  return web_view->AddCookie(*name, url, *cookie_value, domain, path, samesite,
                             secure, http_only, expiry);
}

Status ExecuteDeleteCookie(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  const std::string* name = params.FindString("name");
  if (!name)
    return Status(kInvalidArgument, "missing 'name'");
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
    if (*name == it->name) {
      status = web_view->DeleteCookie(it->name, url, it->domain, it->path);
      if (status.IsError())
        return status;
    }
  }
  return Status(kOk);
}

Status ExecuteDeleteAllCookies(Session* session,
                               WebView* web_view,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  std::list<Cookie> cookies;
  Status status = GetVisibleCookies(session, web_view, &cookies);
  if (status.IsError())
    return status;

  if (!cookies.empty()) {
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

Status ExecuteRunBounceTrackingMitigations(Session* session,
                                           WebView* web_view,
                                           const base::Value::Dict& params,
                                           std::unique_ptr<base::Value>* value,
                                           Timeout* timeout) {
  // Run command and get result
  auto result = std::make_unique<base::Value>(base::Value::Type::DICT);
  Status status = web_view->SendCommandAndGetResult(
      "Storage.runBounceTrackingMitigations", base::Value::Dict(), &result);
  if (status.IsError()) {
    return status;
  }

  if (result->GetDict().empty()) {
    // The result dictionary should only be empty if there is no bounce tracking
    // mitigations service (DIPSService) for the current browser context.
    return Status(
        kUnsupportedOperation,
        "current remote end configuration does not support bounce tracking "
        "mitigations");
  }

  const base::Value::List* deleted_sites =
      result->GetDict().FindList("deletedSites");

  // create copies of items `deleted_sites` and add them to the output list.
  auto site_list = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const base::Value& site : *deleted_sites) {
    if (!site.is_string()) {
      return Status(kUnknownError,
                    "DevTools returns a non-string bounce tracker site");
    }
    site_list->GetList().Append(site.GetString());
  }

  *value = std::move(site_list);

  return Status(kOk);
}

Status ExecuteSetRPHRegistrationMode(Session* session,
                                     WebView* web_view,
                                     const base::Value::Dict& params,
                                     std::unique_ptr<base::Value>* value,
                                     Timeout* timeout) {
  const std::string* mode = params.FindString("mode");
  if (!mode) {
    return Status(kInvalidArgument, "missing parameter 'mode'");
  }

  base::Value::Dict body;
  body.Set("mode", *mode);

  return web_view->SendCommandAndGetResult("Page.setRPHRegistrationMode", body,
                                           value);
}

Status ExecuteSetLocation(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  const base::Value::Dict* location = params.FindDict("location");
  Geoposition geoposition;
  if (!location)
    return Status(kInvalidArgument, "missing or invalid 'location'");

  std::optional<double> maybe_latitude = location->FindDouble("latitude");
  if (!maybe_latitude.has_value())
    return Status(kInvalidArgument, "missing or invalid 'location.latitude'");
  geoposition.latitude = maybe_latitude.value();

  std::optional<double> maybe_longitude = location->FindDouble("longitude");
  if (!maybe_longitude.has_value())
    return Status(kInvalidArgument, "missing or invalid 'location.longitude'");
  geoposition.longitude = maybe_longitude.value();

  // |accuracy| is not part of the WebDriver spec yet, so if it is not given
  // default to 100 meters accuracy.
  std::optional<double> maybe_accuracy =
      ParseDoubleIfInDictionary(*location, "accuracy", 100);
  if (!maybe_accuracy.has_value())
    return Status(kInvalidArgument, "invalid 'accuracy'");
  geoposition.accuracy = maybe_accuracy.value();

  Status status = web_view->OverrideGeolocation(geoposition);
  if (status.IsOk()) {
    session->overridden_geoposition =
        std::make_unique<Geoposition>(geoposition);
  }
  return status;
}

Status ExecuteSetNetworkConditions(Session* session,
                                   WebView* web_view,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value,
                                   Timeout* timeout) {
  const std::string* network_name = params.FindString("network_name");
  std::unique_ptr<NetworkConditions> network_conditions(
      new NetworkConditions());
  if (network_name) {
    // Get conditions from preset list.
    Status status = FindPresetNetwork(*network_name, network_conditions.get());
    if (status.IsError())
      return status;
  } else if (const base::Value::Dict* conditions =
                 params.FindDict("network_conditions")) {
    // |latency| is required.
    std::optional<double> maybe_latency = conditions->FindDouble("latency");
    if (!maybe_latency.has_value())
      return Status(kInvalidArgument,
                    "invalid 'network_conditions' is missing 'latency'");
    network_conditions->latency = maybe_latency.value();

    // Either |throughput| or the pair |download_throughput| and
    // |upload_throughput| is required.
    if (conditions->Find("throughput")) {
      std::optional<double> maybe_throughput =
          conditions->FindDouble("throughput");
      if (!maybe_throughput.has_value())
        return Status(kInvalidArgument, "invalid 'throughput'");
      network_conditions->upload_throughput = maybe_throughput.value();
      network_conditions->download_throughput = maybe_throughput.value();
    } else if (conditions->Find("download_throughput") &&
               conditions->Find("upload_throughput")) {
      std::optional<double> maybe_download_throughput =
          conditions->FindDouble("download_throughput");
      std::optional<double> maybe_upload_throughput =
          conditions->FindDouble("upload_throughput");

      if (!maybe_download_throughput.has_value() ||
          !maybe_upload_throughput.has_value())
        return Status(kInvalidArgument,
                      "invalid 'download_throughput' or 'upload_throughput'");
      network_conditions->download_throughput =
          maybe_download_throughput.value();
      network_conditions->upload_throughput = maybe_upload_throughput.value();
    } else {
      return Status(kInvalidArgument,
                    "invalid 'network_conditions' is missing 'throughput' or "
                    "'download_throughput'/'upload_throughput' pair");
    }

    // |offline| is optional.
    if (const base::Value* offline = conditions->Find("offline")) {
      if (!offline->is_bool())
        return Status(kInvalidArgument, "invalid 'offline'");
      network_conditions->offline = offline->GetBool();
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
                                      const base::Value::Dict& params,
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
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  return web_view->TakeHeapSnapshot(value);
}

Status ExecuteGetWindowRect(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  Chrome::WindowRect window_rect;
  Status status = session->chrome->GetWindowRect(session->window, &window_rect);
  if (status.IsError())
    return status;

  base::Value::Dict rect;
  rect.Set("x", window_rect.x);
  rect.Set("y", window_rect.y);
  rect.Set("width", window_rect.width);
  rect.Set("height", window_rect.height);
  value->reset(new base::Value(std::move(rect)));
  return Status(kOk);
}

Status ExecuteSetWindowRect(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const double max_range = 2147483647;   // 2^31 - 1
  const double min_range = -2147483648;  // -2^31
  const base::Value* temp;
  double width = 0;
  double height = 0;
  double x = 0;
  double y = 0;

  temp = params.Find("x");
  bool has_x = temp && !temp->is_none();
  if (has_x) {
    if (!temp->is_double() && !temp->is_int())
      return Status(kInvalidArgument, "'x' must be a number");
    x = temp->GetDouble();
    if (x > max_range || x < min_range)
      return Status(kInvalidArgument, "'x' out of range");
  }

  temp = params.Find("y");
  bool has_y = temp && !temp->is_none();
  if (has_y) {
    if (!temp->is_double() && !temp->is_int())
      return Status(kInvalidArgument, "'y' must be a number");
    y = temp->GetDouble();
    if (y > max_range || y < min_range)
      return Status(kInvalidArgument, "'y' out of range");
  }

  temp = params.Find("width");
  bool has_width = temp && !temp->is_none();
  if (has_width) {
    if (!temp->is_double() && !temp->is_int())
      return Status(kInvalidArgument, "'width' must be a number");
    width = temp->GetDouble();
    if (width > max_range || width < 0)
      return Status(kInvalidArgument, "'width' out of range");
  }

  temp = params.Find("height");
  bool has_height = temp && !temp->is_none();
  if (has_height) {
    if (!temp->is_double() && !temp->is_int())
      return Status(kInvalidArgument, "'height' must be a number");
    height = temp->GetDouble();
    if (height > max_range || height < 0)
      return Status(kInvalidArgument, "'height' out of range");
  }

  // to pass to the set window rect command
  base::Value::Dict rect_params;
  // only set position if both x and y are given
  if (has_x && has_y) {
    rect_params.Set("x", static_cast<int>(x));
    rect_params.Set("y", static_cast<int>(y));
  }  // only set size if both height and width are given
  if (has_width && has_height) {
    rect_params.Set("width", static_cast<int>(width));
    rect_params.Set("height", static_cast<int>(height));
  }
  Status status = session->chrome->SetWindowRect(session->window, rect_params);
  if (status.IsError())
    return status;

  // return the current window rect
  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteMaximizeWindow(Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  Status status = session->chrome->MaximizeWindow(session->window);
  if (status.IsError())
    return status;

  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteMinimizeWindow(Session* session,
                             WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value,
                             Timeout* timeout) {
  Status status = session->chrome->MinimizeWindow(session->window);
  if (status.IsError())
    return status;

  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteFullScreenWindow(Session* session,
                               WebView* web_view,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  Status status = session->chrome->FullScreenWindow(session->window);
  if (status.IsError())
    return status;

  return ExecuteGetWindowRect(session, web_view, params, value, timeout);
}

Status ExecuteSetSinkToUse(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout) {
  return web_view->SendCommand("Cast.setSinkToUse", params);
}

Status ExecuteStartDesktopMirroring(Session* session,
                                    WebView* web_view,
                                    const base::Value::Dict& params,
                                    std::unique_ptr<base::Value>* value,
                                    Timeout* timeout) {
  return web_view->SendCommand("Cast.startDesktopMirroring", params);
}

Status ExecuteStartTabMirroring(Session* session,
                                WebView* web_view,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout) {
  return web_view->SendCommand("Cast.startTabMirroring", params);
}

Status ExecuteStopCasting(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout) {
  return web_view->SendCommand("Cast.stopCasting", params);
}

Status ExecuteGetSinks(Session* session,
                       WebView* web_view,
                       const base::Value::Dict& params,
                       std::unique_ptr<base::Value>* value,
                       Timeout* timeout) {
  *value = web_view->GetCastSinks();
  return Status(kOk);
}

Status ExecuteGetIssueMessage(Session* session,
                              WebView* web_view,
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout) {
  *value = web_view->GetCastIssueMessage();
  return Status(kOk);
}

Status ExecuteSetPermission(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout) {
  const base::Value::Dict* descriptor = params.FindDict("descriptor");
  if (!descriptor)
    return Status(kInvalidArgument, "no descriptor dictionary");

  const std::string* name = descriptor->FindString("name");
  if (!name)
    return Status(kInvalidArgument, "no name in descriptor");

  const std::string* permission_state = params.FindString("state");
  if (!permission_state)
    return Status(kInvalidArgument, "no permission state");

  Chrome::PermissionState valid_state;
  if (*permission_state == "granted")
    valid_state = Chrome::PermissionState::kGranted;
  else if (*permission_state == "denied")
    valid_state = Chrome::PermissionState::kDenied;
  else if (*permission_state == "prompt")
    valid_state = Chrome::PermissionState::kPrompt;
  else
    return Status(kInvalidArgument, "unrecognized permission state");

  auto dict = std::make_unique<base::Value::Dict>(descriptor->Clone());
  return session->chrome->SetPermission(std::move(dict), valid_state, web_view);
}

Status ExecuteSetDevicePosture(Session* session,
                               WebView* web_view,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value,
                               Timeout* timeout) {
  const std::string* posture = params.FindString("posture");
  if (!posture) {
    return Status(kInvalidArgument, "'posture' must be a string");
  }
  base::Value::Dict args;
  args.Set("posture", base::Value::Dict().Set("type", *posture));
  return web_view->SendCommand("Emulation.setDevicePostureOverride", args);
}

Status ExecuteClearDevicePosture(Session* session,
                                 WebView* web_view,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value,
                                 Timeout* timeout) {
  return web_view->SendCommand("Emulation.clearDevicePostureOverride",
                               base::Value::Dict());
}
